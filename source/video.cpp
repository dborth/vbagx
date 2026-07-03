/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2026
 * softdev 2007
 *
 * video.cpp
 *
 * Video routines
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <ogc/machine/processor.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vbagx.h"
#include "videofilters.h"
#include "menu.h"
#include "input.h"
#include "vbasupport.h"

s32 CursorX, CursorY;
bool CursorVisible;
bool CursorValid;
bool TiltScreen = false;
float TiltAngle = 0;
u32 FrameTimer = 0;

/*** External 2D Video ***/
/*** 2D Video Globals ***/
GXRModeObj *vmode = NULL; // Graphics Mode Object
u32 *xfb[2] = { NULL, NULL }; // Framebuffers
int whichfb = 0; // Frame buffer toggle

static Mtx GXmodelView2D;

GameScreenPng gameScreenPng;

int screenheight = 480;
int screenwidth = 640;

u16 *InitialBorder = NULL;
int InitialBorderWidth = 0;
int InitialBorderHeight = 0;
bool SGBBorderLoadedFromGame = false;

/*** 3D GX ***/
#define DEFAULT_FIFO_SIZE ( 256 * 1024 )
static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN(32);
static volatile unsigned int copynow = GX_FALSE;

/*** Texture memory ***/
#define TEX_WIDTH 640
#define TEX_HEIGHT 480
#define TEXTUREMEM_SIZE 	TEX_WIDTH*TEX_HEIGHT*2
static u8 texturemem[TEXTUREMEM_SIZE] ATTRIBUTE_ALIGN (32);
static unsigned char scanline_tex_data[32] ATTRIBUTE_ALIGN (32);

static GXTexObj texobj;
static GXTexObj cursorObj;
static GXTexObj scanlineTexObj;
static Mtx view;
static int vwidth, vheight;
static int fscale = 1;
static int updateScaling;
bool progressive = false;

/* New texture based scaler */
typedef struct tagcamera
  {
    guVector pos;
    guVector up;
    guVector view;
  }
camera;

/*** Square Matrix
     This structure controls the size of the image on the screen.
***/
static s16 square[] ATTRIBUTE_ALIGN(32) = {
	/*
	* X,   Y,  Z
	* Values set are for roughly 4:3 aspect
	*/
	-200,  200, 0,	// 0
	 200,  200, 0,	// 1
	 200, -200, 0,	// 2
	-200, -200, 0	// 3
    };

// 96x96 static quad for the cursor (Centered at 0,0)
static s16 cursor_square[] ATTRIBUTE_ALIGN(32) = {
	-48,  48, 0,	// 0: Top Left
	 48,  48, 0,	// 1: Top Right
	 48, -48, 0,	// 2: Bottom Right
	-48, -48, 0 	// 3: Bottom Left
};

static camera cam = { {0.0F, 0.0F, 0.0F},
                      {0.0F, 0.5F, 0.0F},
                      {0.0F, 0.0F, -0.5F}
                    };

/****************************************************************************
 * VideoThreading
 ***************************************************************************/
static lwp_t vbthread = LWP_THREAD_NULL;
static lwpq_t render_queue;          // Queue for the main thread to sleep on
static lwpq_t vb_queue;              // Queue for the VSync thread to sleep on
static volatile bool vb_done = true; // Tracks if the VSync thread has completed its wait
static volatile bool vb_wait = false; // Tracks if the VSync thread should begin waiting

/****************************************************************************
 * vbgetback
 *
 * This callback enables the emulator to keep running while waiting for a
 * vertical blank.
 ***************************************************************************/
static void * vbgetback (void *arg)
{
	while (1)
	{
		u32 level;
		_CPU_ISR_Disable(level);
		while (!vb_wait)
		{
			LWP_ThreadSleep(vb_queue);     // Sleep safely until GX_Render kicks us off
		}
		vb_wait = false;
		_CPU_ISR_Restore(level);

		VIDEO_WaitVSync();                 // Wait for video vertical blank

		_CPU_ISR_Disable(level);
		vb_done = true;
		LWP_ThreadSignal(render_queue);    // Instantly alert the main thread
		_CPU_ISR_Restore(level);
	}
	return NULL;
}

/****************************************************************************
 * copy_to_xfb
 *
 * Stock code to copy the GX buffer to the current display mode.
 * Also increments the frameticker, as it's called for each vb.
 ***************************************************************************/
static inline void
copy_to_xfb (u32 arg)
{
	if (copynow == GX_TRUE)
	{
		GX_CopyDisp (xfb[whichfb], GX_TRUE);
		GX_Flush ();
		copynow = GX_FALSE;
		LWP_ThreadSignal(render_queue); // Wake up the main thread if it is waiting for the copy
	}
	++FrameTimer;
}

/****************************************************************************
 * Scanline Support Functions
 ***************************************************************************/

static void InitScanlineTexture() {
	// GX_TF_I8 represents one byte per pixel.
	// We create an 8x4 tile: Rows 0 and 2 are white (0xFF), Rows 1 and 3 are dark (0xA0).
	for (int y = 0; y < 4; y++) {
		u8 intensity = (y % 2 == 0) ? 0xFF : 0xA0; // 0xA0 controls the scanline darkness
		for (int x = 0; x < 8; x++) {
			scanline_tex_data[y * 8 + x] = intensity;
		}
	}

	// CRITICAL: Flush the CPU data cache. GX reads directly from main memory.
	DCStoreRange(scanline_tex_data, 32);

	// Initialize the texture object. Wrap modes MUST be GX_REPEAT to tile across the screen.
	GX_InitTexObj(&scanlineTexObj, scanline_tex_data, 8, 4, GX_TF_I8, GX_REPEAT, GX_REPEAT, GX_FALSE);

	// CRITICAL: Filter mode MUST be GX_NEAR. GX_LINEAR will blur the lines into a muddy gray.
	GX_InitTexObjFilterMode(&scanlineTexObj, GX_NEAR, GX_NEAR);

	// Load the scanline texture into MAP1
	GX_LoadTexObj(&scanlineTexObj, GX_TEXMAP1);
}

static void SetupScanlineFilterTEV() {
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);

	// Allow a second texture coordinate to be passed to the vertex stream
	GX_SetVtxDesc(GX_VA_TEX1, GX_DIRECT);

	// Enable two textures and two TEV stages
	GX_SetNumTexGens(2);
	GX_SetNumTevStages(2);
	GX_SetNumChans(0);

	// Configure Texture Coordinate Generation for both textures
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, GX_IDENTITY);

	// --- STAGE 0: Sample the Game Screen ---
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

	// Configure Stage 0 Alpha path
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

	// --- STAGE 1: Multiply by Scanlines ---
	GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLORNULL);
	// Formula: d + ((1.0 - c) * a + c * b)
	// By setting: a=ZERO, b=CPREV, c=TEXC, d=ZERO -> (TEXC * CPREV)
	GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_CPREV, GX_CC_TEXC, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

	// Configure Stage 1 Alpha path (Pass-through blend)
	GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_APREV, GX_CA_TEXA, GX_CA_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

/****************************************************************************
 * Scaler Support Functions
 ****************************************************************************/
static inline void configure_tev_pipeline()
{
	if(GCSettings.FilterMethod == FILTER_SCANLINES) {
		SetupScanlineFilterTEV();
	}
	else {
		GX_SetNumTexGens (1);
		GX_SetNumTevStages (1);
		GX_SetNumChans (0);

		GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

		GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
		GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	}
}

static inline void draw_init()
{
	GX_ClearVtxDesc ();
	GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	configure_tev_pipeline();

	GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

	memset (&view, 0, sizeof (Mtx));
	guLookAt(view, &cam.pos, &cam.up, &cam.view);
	GX_LoadPosMtxImm (view, GX_PNMTX0);

	GX_InvVtxCache ();	// update vertex cache
}

static inline void draw_vert(u8 pos, f32 s, f32 t)
{
	GX_Position1x8(pos);
	GX_TexCoord2f32(s, t);
}

static inline void draw_square(Mtx v)
{
	Mtx m;			// model matrix.
	Mtx mv;			// modelview matrix.

	if (TiltScreen)
	{
		guMtxRotDeg(m, 'z', -TiltAngle);
		guMtxScaleApply(m, m, 0.8, 0.8, 1);
	}
	else
	{
		guMtxIdentity(m);
	}

	guMtxTransApply(m, m, 0, 0, -100);
	guMtxConcat(v, m, mv);

	GX_LoadPosMtxImm(mv, GX_PNMTX0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	if(GCSettings.FilterMethod == FILTER_SCANLINES) {
		// Calculate physical dimensions of the rendering quad in EFB pixels
		// We use the static 'square' array which holds the final scaled/zoomed screen footprint
		// square[3] and square[0] are the Right and Left X bounds
		// square[1] and square[7] are the Top and Bottom Y bounds
		f32 quad_width = (f32)(square[3] - square[0]);
		f32 quad_height = (f32)(square[1] - square[7]);

		// Map exactly 1 texel to 1 EFB physical TV pixel
		// Our scanline texture is 8 pixels wide and 4 pixels high
		f32 u_repeat = quad_width / 8.0f;
		f32 v_repeat = quad_height / 4.0f;

		// The "Half-Texel Offset" Epsilon.
		// By shifting the UV start coordinates by exactly half a texel, we force the
		// GPU sampler to hit the 'dead center' of the texture pixels (e.g. 0.5, 1.5, 2.5),
		// preventing the moiré effect caused by floating-point edge-rounding.
		// U: 1/8 texel = 0.125. Half of that = 0.0625f
		// V: 1/4 texel = 0.25. Half of that = 0.125f
		f32 u_off = 0.0625f;
		f32 v_off = 0.125f;

		draw_vert (0, 0.0f, 0.0f); // TEX0
		GX_TexCoord2f32 (u_off, v_off); // TEX1

		draw_vert (1, 1.0f, 0.0f); // TEX0
		GX_TexCoord2f32 (u_repeat + u_off, v_off); // TEX1

		draw_vert (2, 1.0f, 1.0f); // TEX0
		GX_TexCoord2f32 (u_repeat + u_off, v_repeat + v_off); // TEX1

		draw_vert (3, 0.0f, 1.0f); // TEX0
		GX_TexCoord2f32 (u_off, v_repeat + v_off); // TEX1
	}
	else {
		draw_vert(0, 0.0, 0.0);
		draw_vert(1, 1.0, 0.0);
		draw_vert(2, 1.0, 1.0);
		draw_vert(3, 0.0, 1.0);
	}
	GX_End();

	if(GCSettings.FilterMethod == FILTER_SCANLINES) {
		// force identity matrix to ensure texture mapping is pristine and devoid of stray scaling
		Mtx texMtx;
		guMtxIdentity(texMtx);
		GX_LoadTexMtxImm(texMtx, GX_TEXMTX1, GX_MTX2x4);
	}
}

#ifdef HW_RVL
static inline void draw_cursor()
{
	if (!CursorVisible || !CursorValid)
		return;

	// --- 1. ISOLATE AND BIND THE CURSOR ARRAY ---
	// We reuse GX_VTXFMT0 to avoid crashing the menu, but we swap the positional array.
	// Flush the static array to main memory to guarantee the GPU reads it correctly.
	DCFlushRange(cursor_square, 32);
	GX_SetArray(GX_VA_POS, cursor_square, 3 * sizeof(s16));

	// --- 2. DISABLE TEX1 ---
	// If scanlines were drawn just before this, TEX1 is required.
	// We must turn it off for the UI pass to prevent a FIFO stall.
	GX_SetVtxDesc(GX_VA_TEX1, GX_NONE);

	// --- 3. CONFIGURE TEV FOR UI ---
	// 1-stage replacement perfectly bypasses any scanline multiplication
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
	GX_SetNumChans(0);

	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);

	// --- 4. CONFIGURE ALPHA BLENDING ---
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_LoadTexObj(&cursorObj, GX_TEXMAP0);

	// --- 5. MATH & POSITIONING ---
	// Map the 0-640 / 0-480 IR coordinates into the -320 to 320 ortho space
	f32 cX = (f32)CursorX - 320.0f;
	f32 cY = 240.0f - (f32)CursorY;

	Mtx m, mv;
	guMtxIdentity(m);
	// Z MUST BE -100.0f
	guMtxTransApply(m, m, cX, cY, -100.0f);
	guMtxConcat(view, m, mv);
	GX_LoadPosMtxImm(mv, GX_PNMTX0);

	// --- 6. DRAW THE CURSOR ---
	// Using the exact same draw_vert logic the game loop uses
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		draw_vert(0, 0.0f, 0.0f);
		draw_vert(1, 1.0f, 0.0f);
		draw_vert(2, 1.0f, 1.0f);
		draw_vert(3, 0.0f, 1.0f);
	GX_End();

	// --- 7. CRITICAL STATE RESTORATION ---
	// Restore array pointer to the game's dynamic scaling square
	GX_SetArray(GX_VA_POS, square, 3 * sizeof(s16));

	// Rebind the game texture to MAP0
	GX_LoadTexObj(&texobj, GX_TEXMAP0);

	// Restore Blending
	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

	// Restore TEV pipeline exactly how the next frame's draw_square expects it
	configure_tev_pipeline();
}
#endif

/****************************************************************************
 * StopGX
 *
 * Stops GX (when exiting)
 ***************************************************************************/
void StopGX()
{
	GX_AbortFrame();
	GX_Flush();

	VIDEO_SetBlack(true);
	VIDEO_Flush();
}

/****************************************************************************
 * FindVideoMode
 *
 * Finds the optimal video mode, or uses the user-specified one
 * Also configures original video modes
 ***************************************************************************/
static GXRModeObj * FindVideoMode()
{
	GXRModeObj * mode;
	
	// choose the desired video mode
	switch(GCSettings.videomode)
	{
		case VIDEOMODE_NTSC: // NTSC (480i)
			mode = &TVNtsc480IntDf;
			break;
		case VIDEOMODE_PROGRESSIVE: // Progressive (480p)
			mode = &TVNtsc480Prog;
			break;
		case VIDEOMODE_PAL: // PAL (50Hz)
			mode = &TVPal576IntDfScale;
			break;
		case VIDEOMODE_EURGB: // PAL (60Hz)
			mode = &TVEurgb60Hz480IntDf;
			break;
		case VIDEOMODE_240P: // NTSC (240p)
			mode = &TVNtsc240Ds;
			break;
		case VIDEOMODE_EURGB_240P: // PAL (60Hz 240p)
			mode = &TVEurgb60Hz240Ds;
			break;
		default:
			mode = VIDEO_GetPreferredMode(NULL);
			break;
	}

	// check for progressive scan
	if ((mode->viTVMode & 3) == VI_PROGRESSIVE)
		progressive = true;
	else
		progressive = false;

	#ifdef HW_RVL
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
		mode->viWidth = 678;
	else
		mode->viWidth = 672;

    if ((mode->viTVMode >> 2) == VI_PAL)
	{
		mode->viXOrigin = (VI_MAX_WIDTH_PAL - mode->viWidth) / 2;
		mode->viYOrigin = (VI_MAX_HEIGHT_PAL - mode->viHeight) / 2;
	}
	else
	{
		mode->viXOrigin = (VI_MAX_WIDTH_NTSC - mode->viWidth) / 2;
		mode->viYOrigin = (VI_MAX_HEIGHT_NTSC - mode->viHeight) / 2;
	}
	#endif
	return mode;
}

/****************************************************************************
 * SetupVideoMode
 *
 * Sets up the given video mode
 ***************************************************************************/
static void SetupVideoMode(GXRModeObj * mode)
{
	if(vmode == mode)
		return;
	
	VIDEO_SetPostRetraceCallback (NULL);
	copynow = GX_FALSE;
	VIDEO_Configure (mode);
	VIDEO_Flush();

	// Clear framebuffers etc.
	VIDEO_ClearFrameBuffer (mode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer (mode, xfb[1], COLOR_BLACK);
	VIDEO_SetNextFramebuffer (xfb[0]);

	VIDEO_SetBlack (false);
	VIDEO_Flush ();
	VIDEO_WaitForFlush ();
	
	VIDEO_SetPostRetraceCallback ((VIRetraceCallback)copy_to_xfb);
	vmode = mode;
}

/****************************************************************************
 * InitializeVideo
 *
 * This function MUST be called at startup.
 * - also sets up menu video mode
 ***************************************************************************/

void
InitializeVideo ()
{
	VIDEO_Init();

	// Allocate the video buffers. Sized for the largest supported mode
	// (640x576, 2 bytes per pixel) so the same buffers serve every mode.
	const u32 xfbSize = 640 * 576 * 2;
	xfb[0] = (u32 *) memalign(32, xfbSize);
	xfb[1] = (u32 *) memalign(32, xfbSize);
	DCInvalidateRange(xfb[0], xfbSize);
	DCInvalidateRange(xfb[1], xfbSize);
	xfb[0] = (u32 *) MEM_K0_TO_K1 (xfb[0]);
	xfb[1] = (u32 *) MEM_K0_TO_K1 (xfb[1]);

	GXRModeObj *rmode = FindVideoMode();

#ifdef HW_RVL
if (CONF_GetAspectRatio() == CONF_ASPECT_16_9 && (*(u32*)(0xCD8005A0) >> 16) == 0xCAFE) // Wii U
{
	/* vWii widescreen patch by tueidj */
	write32(0xd8006a0, 0x30000004), mask32(0xd8006a8, 0, 2);
}
#endif

	SetupVideoMode(rmode);

	// Setup synchronization queues
	LWP_InitQueue(&render_queue);
	LWP_InitQueue(&vb_queue);
	vb_done = true;
	LWP_CreateThread (&vbthread, vbgetback, NULL, NULL, 0, 68);

	// Initialise GX
	GXColor background = { 0, 0, 0, 0xff };
	memset (gp_fifo, 0, DEFAULT_FIFO_SIZE);
	GX_Init (&gp_fifo, DEFAULT_FIFO_SIZE);
	GX_SetCopyClear (background, GX_MAX_Z24);
	GX_SetDispCopyGamma (GX_GM_1_0);
	GX_SetCullMode (GX_CULL_NONE);
}

static inline void UpdateScaling()
{
	int xscale;
	int yscale;

	float TvAspectRatio;
	float GameboyAspectRatio;
	float MaxStretchRatio = 1.6f;

	if (GCSettings.scaling == SCALING_PARTIAL_STRETCH)
		MaxStretchRatio = 1.3f;
	else if (GCSettings.scaling == SCALING_STRETCH_TO_FIT)
		MaxStretchRatio = 1.6f;
	else
		MaxStretchRatio = 1.0f;

	#ifdef HW_RVL
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
		TvAspectRatio = 16.0f/9.0f;
	else
		TvAspectRatio = 4.0f/3.0f;
	#else
	if (GCSettings.scaling == SCALING_WIDESCREEN_CORRECTION)
		TvAspectRatio = 16.0f/9.0f;
	else
		TvAspectRatio = 4.0f/3.0f;
	#endif

	GameboyAspectRatio = ((vwidth * 1.0) / vheight);

	if (TvAspectRatio>GameboyAspectRatio)
	{
		yscale = 240; // half of TV resolution 640x480
		float StretchRatio = TvAspectRatio/GameboyAspectRatio;
		if (StretchRatio > MaxStretchRatio)
			StretchRatio = MaxStretchRatio;
		xscale = 240.0f*GameboyAspectRatio*StretchRatio * ((4.0f/3.0f)/TvAspectRatio);
	}
	else
	{
		xscale = 320; // half of TV resolution 640x480
		float StretchRatio = GameboyAspectRatio/TvAspectRatio;
		if (StretchRatio > MaxStretchRatio)
			StretchRatio = MaxStretchRatio;
		yscale = 320.0f/GameboyAspectRatio*StretchRatio / ((4.0f/3.0f)/TvAspectRatio);
	}

	// change zoom
	float zoomHor, zoomVert;
	int fixed;
	if (cartridgeType == CARTRIDGE_GBA)
	{
		zoomHor = GCSettings.gbaZoomHor;
		zoomVert = GCSettings.gbaZoomVert;
		fixed = GCSettings.gbaFixed;
	}
	else
	{
		zoomHor = GCSettings.gbZoomHor;
		zoomVert = GCSettings.gbZoomVert;
		fixed = GCSettings.gbFixed;
	}

	if (fixed) {
		xscale = 320;
		yscale = 240;
	} else {
		xscale *= zoomHor;
		yscale *= zoomVert;
	}

	// Set new aspect
	square[0] = square[9]  = -xscale + GCSettings.xshift;
	square[3] = square[6]  =  xscale + GCSettings.xshift;
	square[1] = square[4]  =  yscale - GCSettings.yshift;
	square[7] = square[10] = -yscale - GCSettings.yshift;
	DCFlushRange (square, 32); // update memory BEFORE the GPU accesses it!

	float targetWidth = screenwidth * (2.0f * xscale) / (float)vmode->fbWidth;
	float targetHeight = screenheight * (2.0f * yscale) / (float)vmode->efbHeight;
	gameScreenPng.width = vwidth * fscale;
	gameScreenPng.height = vheight * fscale;
	gameScreenPng.scaleX = targetWidth / (float)gameScreenPng.width;
	gameScreenPng.scaleY = targetHeight / (float)gameScreenPng.height;
	gameScreenPng.xoffset = (GCSettings.xshift * screenwidth) / vmode->fbWidth;
	gameScreenPng.yoffset = (GCSettings.yshift * screenheight) / vmode->efbHeight;

	draw_init ();

	memset(&view, 0, sizeof(Mtx));
	guLookAt(view, &cam.pos, &cam.up, &cam.view);
	if (fixed) {
		int ratio = fixed % 10;
		bool widescreen = fixed / 10;
	
		float vw = vwidth * ratio;
		if (widescreen) vw /= 4.0 / 3.0;
		float vh = vheight * ratio;
		
		// 240p adjustment
		if (GCSettings.videomode == VIDEOMODE_240P || GCSettings.videomode == VIDEOMODE_EURGB_240P) vw *= 2;
		
		float vx = (vmode->fbWidth - vw) / 2;
		float vy = (vmode->efbHeight - vh) / 2;
		GX_SetViewport(vx, vy, vw, vh, 0, 1);
	} else {
		GX_SetViewport(0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	}

	updateScaling = 0;
}

/****************************************************************************
 * ResetVideo_Emu
 *
 * Reset the video/rendering mode for the emulator rendering
****************************************************************************/
void
ResetVideo_Emu ()
{
	Mtx44 p;
	GXRModeObj * rmode = FindVideoMode();

	SetupVideoMode(rmode); // reconfigure VI

	// reconfigure GX
	GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale ((f32) rmode->xfbHeight / (f32) rmode->efbHeight);
	GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);

	GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst (rmode->fbWidth, rmode->xfbHeight);
	u8 sharp[7] = {0,0,21,22,21,0,0};
	u8 soft[7] = {8,8,10,12,10,8,8};
	u8* vfilter =
		GCSettings.render == RENDER_FILTERED_SHARP ? sharp
		: GCSettings.render == RENDER_FILTERED_SOFT ? soft
		: rmode->vfilter;
	GX_SetCopyFilter (rmode->aa, rmode->sample_pattern, (GCSettings.render != RENDER_UNFILTERED) ? GX_TRUE : GX_FALSE, vfilter);	// deflickering filter only for filtered mode

	GX_SetFieldMode (rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	
	if (rmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	
	GX_SetCullMode (GX_CULL_NONE);
	GX_SetDispCopyGamma (GX_GM_1_0);

	GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate (GX_TRUE);
	GX_SetBlendMode (GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

	guOrtho(p, 480/2, -(480/2), -(640/2), 640/2, 100, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

	fscale = GetFilterScale();
	draw_init();
	// set aspect ratio
	updateScaling = 1;
}

static const u16* lastCopiedBorder = NULL;
static int sgbBorderCheckCounter = 0;
static bool borderJustChanged = false;

/****************************************************************************
 * GX_Render_Init
 ***************************************************************************/
void GX_Render_Init(int width, int height) {
	memset(texturemem, 0, TEXTUREMEM_SIZE);

	/*** Setup for first call to scaler ***/
	vwidth = width;
	vheight = height;

	// Reset state trackers upon texture recreation
	lastCopiedBorder = NULL;
	sgbBorderCheckCounter = 0;
	borderJustChanged = false;
}

static bool borderAreaEmpty(const u16* buffer) {
	u16 reference = buffer[0];
	for (int y = 0; y < 40; y++) {
		for (int x = 0; x < 256; x++) {
			if (buffer[256 * y + x] != reference)
				return false;
		}
	}
	for (int y = 40; y < 184; y++) {
		for (int x = 0; x < 48; x++) {
			if (buffer[256 * y + x] != reference)
				return false;
		}
		for (int x = 208; x < 224; x++) {
			if (buffer[256 * y + x] != reference)
				return false;
		}
	}
	for (int y = 184; y < 224; y++) {
		for (int x = 0; x < 256; x++) {
			if (buffer[256 * y + x] != reference)
				return false;
		}
	}
	return true;
}

/****************************************************************************
 * ProcessSGBBorder
 ***************************************************************************/
static void ProcessSGBBorder(u8* buffer, int gbWidth, int gbHeight) {
	if (gbWidth == 256 && gbHeight == 224 && !SGBBorderLoadedFromGame) {
		// Throttle heavy pixel scanning path to once per second
		sgbBorderCheckCounter++;
		if (sgbBorderCheckCounter >= 60) {
			sgbBorderCheckCounter = 0;
			if (!borderAreaEmpty((u16*)buffer)) {
				// don't try to load the default border anymore
				SGBBorderLoadedFromGame = true;
				SaveSGBBorderIfNoneExists(buffer);
			}
		}
	}
}

/****************************************************************************
 * DrawBorderAndGetDest
 ***************************************************************************/
static long long int* DrawBorderAndGetDest(void* textureBase, int gbWidth, int gbHeight, int borderWidth, int borderHeight) {
	long long int* dst = (long long int*) textureBase;
	borderJustChanged = false;

	if (InitialBorder) {
		// Only copy the 600 KB border once when it changes!
		if (InitialBorder != lastCopiedBorder) {
			memcpy(dst, InitialBorder, borderWidth * borderHeight * 2);
			lastCopiedBorder = InitialBorder;
			borderJustChanged = true; // Signal that the GPU needs a full RAM sync
		}

		int rows_to_skip = (borderHeight - gbHeight) / 2;
		if (rows_to_skip > 0)
			dst += rows_to_skip * borderWidth / 4;
		dst += (borderWidth - gbWidth) / 2;
	} else {
		lastCopiedBorder = NULL; // Reset tracking if running borderless
	}

	return dst;
}

/****************************************************************************
 * MakeTextureVBA
 *
 * High-performance texture swizzling (Linear to 4x4 Tiled)
 * specifically optimized for VBA GX's dynamic widths and border gaps.
 * Uses a pipelined 32-bit integer strategy to strictly enforce 4-byte
 * alignment.
 ***************************************************************************/
/****************************************************************************
 * MakeTextureVBA_Impl (Static Template)
 *
 * Employs compile-time constants to embed memory offsets directly into
 * the PowerPC load instructions. Completely eliminates pointer arithmetic
 * from the inner loop.
 ***************************************************************************/
template <int PITCH>
static inline void MakeTextureVBA_Impl(const void *src, void *dst, s32 width, s32 height, s32 dst_gap_bytes)
{
    u32 src_row_stride = PITCH * 4;
    u32 r_src_row;
    u32 tmpA, tmpB, tmpC, tmpD;

    __asm__ __volatile__ (
        "srwi   %[width], %[width], 2\n"
        "srwi   %[height], %[height], 2\n"

    "2: mtctr   %[width]\n"
        "mr     %[r_src_row], %[src]\n"

    "1: dcbz    0, %[dst]\n"

        // Load Row 0
        "lwz    %[tmpA], 0(%[src])\n"
        "lwz    %[tmpB], 4(%[src])\n"

        // Load Row 1, Store Row 0
        "lwz    %[tmpC], %c[p1](%[src])\n"
        "stw    %[tmpA], 0(%[dst])\n"
        "lwz    %[tmpD], %c[p1_4](%[src])\n"
        "stw    %[tmpB], 4(%[dst])\n"

        // Load Row 2, Store Row 1
        "lwz    %[tmpA], %c[p2](%[src])\n"
        "stw    %[tmpC], 8(%[dst])\n"
        "lwz    %[tmpB], %c[p2_4](%[src])\n"
        "stw    %[tmpD], 12(%[dst])\n"

        // Load Row 3, Store Row 2
        "lwz    %[tmpC], %c[p3](%[src])\n"
        "stw    %[tmpA], 16(%[dst])\n"
        "lwz    %[tmpD], %c[p3_4](%[src])\n"
        "stw    %[tmpB], 20(%[dst])\n"

        // Store Row 3
        "stw    %[tmpC], 24(%[dst])\n"
        "stw    %[tmpD], 28(%[dst])\n"

        // Advance inner loop (X)
        "addi   %[src], %[src], 8\n"
        "addi   %[dst], %[dst], 32\n"
        "bdnz   1b\n"

        // Advance outer loop (Y)
        "add    %[src], %[r_src_row], %[src_row_stride]\n"
        "add    %[dst], %[dst], %[dst_gap_bytes]\n"

        "subic. %[height], %[height], 1\n"
        "bne    2b\n"

        : [r_src_row] "=&b" (r_src_row),
          [tmpA] "=&r" (tmpA),
          [tmpB] "=&r" (tmpB),
          [tmpC] "=&r" (tmpC),
          [tmpD] "=&r" (tmpD),
          [src] "+b" (src),
          [dst] "+b" (dst),
          [width] "+r" (width),
          [height] "+r" (height)
        : [p1] "n" (PITCH),
          [p1_4] "n" (PITCH + 4),
          [p2] "n" (PITCH * 2),
          [p2_4] "n" (PITCH * 2 + 4),
          [p3] "n" (PITCH * 3),
          [p3_4] "n" (PITCH * 3 + 4),
          [src_row_stride] "r" (src_row_stride),
          [dst_gap_bytes] "r" (dst_gap_bytes)
        : "memory", "cc"
    );
}

// Fallback for custom/bizarre resolutions (Uses dynamic row_ptr)
static void MakeTextureVBA_Dynamic(const void *src, void *dst, s32 width, s32 height, s32 pitch, s32 dst_gap_bytes)
{
    u32 src_row_stride = pitch * 4;
    u32 r_src_row, row_ptr;
    u32 tmpA, tmpB, tmpC, tmpD;

    __asm__ __volatile__ (
        "srwi   %[width], %[width], 2\n"       // num_tiles_x = width / 4
        "srwi   %[height], %[height], 2\n"     // num_tiles_y = height / 4

    "2: mtctr   %[width]\n"                    // Set inner loop counter (X)
        "mr     %[r_src_row], %[src]\n"        // Save the start of the current source 4-row block

    "1: dcbz    0, %[dst]\n"                   // ZERO L1 CACHE: Dest is perfectly 32-byte aligned
        "mr     %[row_ptr], %[src]\n"

        // Load Row 0
        "lwz    %[tmpA], 0(%[row_ptr])\n"
        "lwz    %[tmpB], 4(%[row_ptr])\n"
        "add    %[row_ptr], %[row_ptr], %[pitch]\n"

        // Load Row 1, Store Row 0
        // Interleaving hides the 3-cycle load latency
        "lwz    %[tmpC], 0(%[row_ptr])\n"
        "stw    %[tmpA], 0(%[dst])\n"
        "lwz    %[tmpD], 4(%[row_ptr])\n"
        "stw    %[tmpB], 4(%[dst])\n"
        "add    %[row_ptr], %[row_ptr], %[pitch]\n"

        // Load Row 2, Store Row 1
        "lwz    %[tmpA], 0(%[row_ptr])\n"      // Recycle tmpA and tmpB
        "stw    %[tmpC], 8(%[dst])\n"
        "lwz    %[tmpB], 4(%[row_ptr])\n"
        "stw    %[tmpD], 12(%[dst])\n"
        "add    %[row_ptr], %[row_ptr], %[pitch]\n"

        // Load Row 3, Store Row 2
        "lwz    %[tmpC], 0(%[row_ptr])\n"
        "stw    %[tmpA], 16(%[dst])\n"
        "lwz    %[tmpD], 4(%[row_ptr])\n"
        "stw    %[tmpB], 20(%[dst])\n"

        // Store Row 3
        "stw    %[tmpC], 24(%[dst])\n"
        "stw    %[tmpD], 28(%[dst])\n"

        // Advance pointers for the next tile in the row
        "addi   %[src], %[src], 8\n"           // Advance src X by 4 pixels (8 bytes)
        "addi   %[dst], %[dst], 32\n"          // Advance dst by 1 full tile (32 bytes)
        "bdnz   1b\n"                          // Decrement CTR, loop inner if > 0

        // Advance pointers to the next row of tiles
        "add    %[src], %[r_src_row], %[src_row_stride]\n" // Jump down 4 source rows
        "add    %[dst], %[dst], %[dst_gap_bytes]\n"        // Skip right/left borders in dest

        "subic. %[height], %[height], 1\n"     // Decrement height counter (Y)
        "bne    2b\n"                          // Loop outer if > 0

        : [r_src_row] "=&b" (r_src_row),
          [row_ptr] "=&b" (row_ptr),
          [tmpA] "=&r" (tmpA),
          [tmpB] "=&r" (tmpB),
          [tmpC] "=&r" (tmpC),
          [tmpD] "=&r" (tmpD),
          [src] "+b" (src),
          [dst] "+b" (dst),
          [width] "+r" (width),
          [height] "+r" (height)
        : [pitch] "r" (pitch),
          [src_row_stride] "r" (src_row_stride),
          [dst_gap_bytes] "r" (dst_gap_bytes)
        : "memory", "cc"
    );
}

/****************************************************************************
 * WriteFrameToTextureMemory
 ****************************************************************************/
static void WriteFrameToTextureMemory(u8* srcBuffer, void* textureBase, int width, int height)
{
    int borderWidth = InitialBorder ? InitialBorderWidth : width;
    int borderHeight = InitialBorder ? InitialBorderHeight : height;

    ProcessSGBBorder(srcBuffer, width, height);
    long long int* dst_ptr = DrawBorderAndGetDest(textureBase, width, height, borderWidth, borderHeight);

    int gbPitch = width * 2 + 4;
    int dst_gap_bytes = ((borderWidth - width) / 4) * 32;

    // Route to the statically optimized ASM based on console resolution width
    switch (width) {
        case 160: // GB / GBC (Pitch = 324)
            MakeTextureVBA_Impl<324>(srcBuffer, dst_ptr, width, height, dst_gap_bytes);
            break;
        case 240: // GBA (Pitch = 484)
            MakeTextureVBA_Impl<484>(srcBuffer, dst_ptr, width, height, dst_gap_bytes);
            break;
        case 256: // SGB (Pitch = 516)
            MakeTextureVBA_Impl<516>(srcBuffer, dst_ptr, width, height, dst_gap_bytes);
            break;
        default:  // Fallback for custom borders/resolutions
            MakeTextureVBA_Dynamic(srcBuffer, dst_ptr, width, height, gbPitch, dst_gap_bytes);
            break;
    }

    // High-efficiency targeted data cache flushing
    if (InitialBorder && !borderJustChanged) {
        // Normal Frame: Flush ONLY the game screen cache lines (Saves ~87% bus bandwidth)
        u8* flush_ptr = (u8*)dst_ptr;
        u32 row_bytes = width * 8;         // bytes per tile row for game screen
        u32 stride_bytes = borderWidth * 8; // full texture pitch stride bytes
        int tile_rows = height / 4;
        for (int i = 0; i < tile_rows; i++) {
            DCStoreRange(flush_ptr, row_bytes);
            flush_ptr += stride_bytes;
        }
    } else {
        // Flush everything if borderless, OR if the border was just copied this frame
        DCStoreRange(textureBase, borderWidth * borderHeight * 2);
    }
}

/****************************************************************************
 * GX_Render
 *
 * Pass in a buffer, width and height to update as a tiled RGB565 texture
 * (2 bytes per pixel)
 ****************************************************************************/
void GX_Render(int consoleWidth, int consoleHeight, u8 * buffer)
{
	// Disable custom exterior borders if CPU filtering to prevent scaling overlap
	bool useBorder = (InitialBorder != NULL) && (fscale == 1);
	int borderWidth = useBorder ? InitialBorderWidth : consoleWidth;
	int borderHeight = useBorder ? InitialBorderHeight : consoleHeight;

	// Only trigger a scaling/texture re-init if the dimensions or scale actually changed
	if (vwidth != borderWidth || vheight != borderHeight || updateScaling) {
		vwidth = borderWidth;
		vheight = borderHeight;
		updateScaling = 1;
	}

	// Ensure previous frame copy and background VSync block have finished cleanly
	u32 level;

	_CPU_ISR_Disable(level);
	while (!vb_done || (copynow == GX_TRUE))
	{
		LWP_ThreadSleep(render_queue); // Halts main thread with 0 CPU load until signals occur
	}
	_CPU_ISR_Restore(level);

	whichfb ^= 1;

	if (updateScaling) {
		UpdateScaling();

		GX_InitTexObj(&texobj, texturemem, vwidth * fscale, vheight * fscale, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);

		if (GCSettings.render == RENDER_UNFILTERED)
			GX_InitTexObjFilterMode(&texobj,GX_NEAR,GX_NEAR); // original/unfiltered video mode: force texture filtering OFF

		GX_LoadTexObj(&texobj, GX_TEXMAP0);

		if(GCSettings.FilterMethod == FILTER_SCANLINES)
			InitScanlineTexture();

		#ifdef HW_RVL
		GX_InitTexObj(&cursorObj, pointer[0]->GetImage(), 96, 96, GX_TF_RGBA8,GX_CLAMP, GX_CLAMP,GX_FALSE);
		#endif
	}

	if (fscale > 1) {
		// Calculate the VBA-M core pitch ((width * 2) + 4)
		int gbPitch = (consoleWidth * 2) + 4;

		FilterMethod(buffer, gbPitch, texturemem, consoleWidth * fscale * 2, consoleWidth, consoleHeight);

		// Pad flush size cleanly to 32-byte cache line boundaries
		u32 padded_width = (consoleWidth * fscale + 3) & ~3;
		u32 padded_height = (consoleHeight * fscale + 3) & ~3;
		DCStoreRange(texturemem, padded_width * padded_height * 2);
	}
	else {
		WriteFrameToTextureMemory(buffer, texturemem, consoleWidth, consoleHeight);
	}

	GX_InvalidateTexAll();

	draw_square(view); // render textured quad

	#ifdef HW_RVL
	draw_cursor(); // render cursor
	#endif
	GX_DrawDone();

	if(ScreenshotRequested)
	{
		ScreenshotRequested = 0;
		TakeScreenshot();
		ConfigRequested = 1;
	}

	// EFB is ready to be copied into XFB
	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();
	copynow = GX_TRUE;

	// Reset state and signal background VSync thread to begin waiting for next blanking interval
	_CPU_ISR_Disable(level);
	vb_done = false;
	vb_wait = true;
	LWP_ThreadSignal(vb_queue);
	_CPU_ISR_Restore(level);
}

void ClearScreenshot()
{
	if(gameScreenPng.buffer) {
		free(gameScreenPng.buffer);
		gameScreenPng.buffer = NULL;
	}

	gameScreenPng.size = 0;
}

/****************************************************************************
 * TakeScreenshot
 *
 * Copies the current texturemem screen into a PNG buffer
 ***************************************************************************/
void TakeScreenshot()
{
	IMGCTX pngContext = PNGU_SelectImageFromBuffer(savebuffer);

	if (pngContext == NULL) {
		return;
	}

	gameScreenPng.size = PNGU_EncodeFromGXTexture(pngContext, gameScreenPng.width, gameScreenPng.height, texturemem, gameScreenPng.width * 3);
	PNGU_ReleaseImageContext(pngContext);

	if (gameScreenPng.size <= 0) {
		ClearScreenshot();
		return;
	}

	gameScreenPng.buffer = (u8 *) malloc(gameScreenPng.size);
	if (gameScreenPng.buffer == NULL) {
		ClearScreenshot();
		return;
	}
	memcpy(gameScreenPng.buffer, savebuffer, gameScreenPng.size);
}

/****************************************************************************
 * ResetVideo_Menu
 *
 * Reset the video/rendering mode for the menu
****************************************************************************/
void
ResetVideo_Menu ()
{
	Mtx44 p;
	f32 yscale;
	u32 xfbHeight;
	GXRModeObj * rmode = FindVideoMode();

	SetupVideoMode(rmode); // reconfigure VI

	// clears the bg to color and clears the z buffer
	GXColor background = {0, 0, 0, 255};
	GX_SetCopyClear (background, GX_MAX_Z24);

	yscale = GX_GetYScaleFactor(vmode->efbHeight,vmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetDispCopySrc(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetDispCopyDst(vmode->fbWidth,xfbHeight);
	GX_SetCopyFilter(vmode->aa,vmode->sample_pattern,GX_TRUE,vmode->vfilter);
	GX_SetFieldMode(vmode->field_rendering,((vmode->viHeight==2*vmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	if (vmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_InvVtxCache ();
	GX_InvalidateTexAll();

	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc (GX_VA_CLR0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_TRUE);

	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);

	GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	guMtxIdentity(GXmodelView2D);
	guMtxTransApply (GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -50.0F);
	GX_LoadPosMtxImm(GXmodelView2D,GX_PNMTX0);

	guOrtho(p,0,479,0,639,0,300);
	GX_LoadProjectionMtx(p, GX_ORTHOGRAPHIC);

	GX_SetViewport(0,0,vmode->fbWidth,vmode->efbHeight,0,1);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetAlphaUpdate(GX_TRUE);
}

/****************************************************************************
 * Menu_Render
 *
 * Renders everything current sent to GX, and flushes video
 ***************************************************************************/
void Menu_Render()
{
	whichfb ^= 1; // flip framebuffer
	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);
	GX_CopyDisp(xfb[whichfb],GX_TRUE);
	GX_DrawDone();
	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();
	VIDEO_WaitForFlush();
}

/****************************************************************************
 * Menu_DrawImg
 *
 * Draws the specified image on screen using GX
 ***************************************************************************/
void Menu_DrawImg(f32 xpos, f32 ypos, u16 width, u16 height, u8 data[],
	f32 degrees, f32 scaleX, f32 scaleY, u8 alpha)
{
	if(data == NULL)
		return;

	GXTexObj texObj;

	GX_InitTexObj(&texObj, data, width,height, GX_TF_RGBA8,GX_CLAMP, GX_CLAMP,GX_FALSE);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);
	GX_InvalidateTexAll();

	GX_SetTevOp (GX_TEVSTAGE0, GX_MODULATE);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	Mtx m,m1,m2, mv;
	width  >>= 1;
	height >>= 1;

	guMtxIdentity (m1);
	guMtxScaleApply(m1,m1,scaleX,scaleY,1.0);
	guVector axis = (guVector) {0 , 0, 1 };
	guMtxRotAxisDeg (m2, &axis, degrees);
	guMtxConcat(m2,m1,m);

	guMtxTransApply(m,m, xpos+width,ypos+height,0);
	guMtxConcat (GXmodelView2D, m, mv);
	GX_LoadPosMtxImm (mv, GX_PNMTX0);

	GX_Begin(GX_QUADS, GX_VTXFMT0,4);
	GX_Position3f32(-width, -height,  0);
	GX_Color4u8(0xFF,0xFF,0xFF,alpha);
	GX_TexCoord2f32(0, 0);

	GX_Position3f32(width, -height,  0);
	GX_Color4u8(0xFF,0xFF,0xFF,alpha);
	GX_TexCoord2f32(1, 0);

	GX_Position3f32(width, height,  0);
	GX_Color4u8(0xFF,0xFF,0xFF,alpha);
	GX_TexCoord2f32(1, 1);

	GX_Position3f32(-width, height,  0);
	GX_Color4u8(0xFF,0xFF,0xFF,alpha);
	GX_TexCoord2f32(0, 1);
	GX_End();
	GX_LoadPosMtxImm (GXmodelView2D, GX_PNMTX0);

	GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetVtxDesc (GX_VA_TEX0, GX_NONE);
}

/****************************************************************************
 * Menu_DrawRectangle
 *
 * Draws a rectangle at the specified coordinates using GX
 ***************************************************************************/
void Menu_DrawRectangle(f32 x, f32 y, f32 width, f32 height, GXColor color, u8 filled)
{
	long n = 4;
	f32 x2 = x+width;
	f32 y2 = y+height;
	guVector v[] = {{x,y,0.0f}, {x2,y,0.0f}, {x2,y2,0.0f}, {x,y2,0.0f}, {x,y,0.0f}};
	u8 fmt = GX_TRIANGLEFAN;

	if(!filled)
	{
		fmt = GX_LINESTRIP;
		n = 5;
	}

	GX_Begin(fmt, GX_VTXFMT0, n);
	for(long i=0; i<n; ++i)
	{
		GX_Position3f32(v[i].x, v[i].y,  v[i].z);
		GX_Color4u8(color.r, color.g, color.b, color.a);
	}
	GX_End();
}

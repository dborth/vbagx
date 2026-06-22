/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2023
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

u8 * gameScreenPng = NULL;
int gameScreenPngSize = 0;

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

static GXTexObj texobj;
static Mtx view;
static int vwidth, vheight;
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

/****************************************************************************
 * vbgetback
 *
 * This callback enables the emulator to keep running while waiting for a
 * vertical blank.
 ***************************************************************************/
static void *
vbgetback (void *arg)
{
	while (1)
	{
		LWP_ThreadSleep(vb_queue);     // Sleep until kicked off at the end of GX_Render
		VIDEO_WaitVSync ();	         /**< Wait for video vertical blank */
		vb_done = true;
		LWP_ThreadSignal(render_queue); // Instantly alert the main thread if it is waiting
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
 * Scaler Support Functions
 ****************************************************************************/
static inline void draw_init(void)
{
	GX_ClearVtxDesc ();
	GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

	GX_SetNumTexGens (1);
	GX_SetNumChans (0);

	GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);

	memset (&view, 0, sizeof (Mtx));
	guLookAt(view, &cam.pos, &cam.up, &cam.view);
	GX_LoadPosMtxImm (view, GX_PNMTX0);

	GX_InvVtxCache ();	// update vertex cache

	GX_InitTexObj(&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);

	if (GCSettings.render == RENDER_UNFILTERED)
		GX_InitTexObjFilterMode(&texobj,GX_NEAR,GX_NEAR); // original/unfiltered video mode: force texture filtering OFF
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
	draw_vert(0, 0.0, 0.0);
	draw_vert(1, 1.0, 0.0);
	draw_vert(2, 1.0, 1.0);
	draw_vert(3, 0.0, 1.0);
	GX_End();
}

#ifdef HW_RVL
static inline void draw_cursor(Mtx v)
{
	if (!CursorVisible || !CursorValid)
		return;

	GX_InitTexObj(&texobj, pointer[0]->GetImage(), 96, 96, GX_TF_RGBA8,GX_CLAMP, GX_CLAMP,GX_FALSE);
	GX_LoadTexObj(&texobj, GX_TEXMAP0);
	GX_SetBlendMode(GX_BM_BLEND,GX_BL_DSTALPHA,GX_BL_INVSRCALPHA,GX_LO_CLEAR);
	GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	Mtx m;			// model matrix.

	guMtxIdentity(m);
	guMtxScaleApply(m, m, 0.070f, 0.10f, 0.06f);
	// I needed to hack this position
	guMtxTransApply(m, m, CursorX-315, 220-CursorY, -100);

	GX_LoadPosMtxImm(m, GX_PNMTX0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	// I needed to hack the texture coords to cut out the opaque bit around the outside
	draw_vert(0, 0.4, 0.45);
	draw_vert(1, 0.76, 0.45);
	draw_vert(2, 0.76, 0.97);
	draw_vert(3, 0.4, 0.97);

	GX_End();

	GX_ClearVtxDesc ();
	GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

	GX_SetNumTexGens (1);
	GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_InvVtxCache ();	// update vertex cache

	GX_InitTexObj(&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);

	if (GCSettings.render == RENDER_UNFILTERED)
		GX_InitTexObjFilterMode(&texobj,GX_NEAR,GX_NEAR); // original/unfiltered video mode: force texture filtering OFF
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
	
	#ifdef HW_RVL
	if (fixed && CONF_GetAspectRatio() == CONF_ASPECT_16_9 && (*(u32*)(0xCD8005A0) >> 16) == 0xCAFE) // Wii U
	{
		/* vWii widescreen patch by tueidj */
		write32(0xd8006a0, fixed ? 0x30000002 : 0x30000004), mask32(0xd8006a8, 0, 2);
	}
	#endif

	// Set new aspect
	square[0] = square[9]  = -xscale + GCSettings.xshift;
	square[3] = square[6]  =  xscale + GCSettings.xshift;
	square[1] = square[4]  =  yscale - GCSettings.yshift;
	square[7] = square[10] = -yscale - GCSettings.yshift;
	DCFlushRange (square, 32); // update memory BEFORE the GPU accesses it!

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
	GX_SetBlendMode(GX_BM_BLEND,GX_BL_DSTALPHA,GX_BL_INVSRCALPHA,GX_LO_CLEAR);

	guOrtho(p, 480/2, -(480/2), -(640/2), 640/2, 100, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

	// reinitialize texture
	GX_InvalidateTexAll ();
	GX_InitTexObj (&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);	// initialize the texture obj we are going to use

	if (GCSettings.render == RENDER_UNFILTERED)
		GX_InitTexObjFilterMode(&texobj,GX_NEAR,GX_NEAR); // original/unfiltered video mode: force texture filtering OFF

	GX_Flush();
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
void WriteFrameToTextureMemory(u8* srcBuffer, void* textureBase, int width, int height)
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
void GX_Render(int gbWidth, int gbHeight, u8 * buffer)
{
	int borderWidth = InitialBorder ? InitialBorderWidth : gbWidth;
	int borderHeight = InitialBorder ? InitialBorderHeight : gbHeight;

	vwidth = borderWidth;
	vheight = borderHeight;

	// Ensure previous frame copy and background VSync block have finished cleanly
	while (!vb_done || (copynow == GX_TRUE))
	{
		LWP_ThreadSleep(render_queue); // Halts main thread with 0 CPU load until signals occur
	}

	whichfb ^= 1;

	if(updateScaling)
		UpdateScaling();

	GX_InvalidateTexAll();
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);

	// load texture into GX
	WriteFrameToTextureMemory(buffer, texturemem, gbWidth, gbHeight);
	
	GX_SetNumChans(1);
	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);
	GX_LoadTexObj(&texobj, GX_TEXMAP0);

	draw_square(view); // render textured quad
	#ifdef HW_RVL
	draw_cursor(view); // render cursor
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
	vb_done = false;
	LWP_ThreadSignal(vb_queue);
}

/****************************************************************************
 * TakeScreenshot
 *
 * Copies the current screen into a GX texture
 ***************************************************************************/
void TakeScreenshot()
{
	IMGCTX pngContext = PNGU_SelectImageFromBuffer(savebuffer);

	if (pngContext != NULL)
	{
		gameScreenPngSize = PNGU_EncodeFromEFB(pngContext, vmode->fbWidth, vmode->efbHeight);
		PNGU_ReleaseImageContext(pngContext);

		if (gameScreenPngSize <= 0) {
			gameScreenPngSize = 0;
			return;
		}

		gameScreenPng = (u8 *) malloc(gameScreenPngSize);
		if (gameScreenPng == NULL) {
			gameScreenPngSize = 0;
			return;
		}
		memcpy(gameScreenPng, savebuffer, gameScreenPngSize);
	}
}

void ClearScreenshot()
{
	if(gameScreenPng)
	{
		gameScreenPngSize = 0;
		free(gameScreenPng);
		gameScreenPng = NULL;
	}
}

/****************************************************************************
 * ResetVideo_Menu
 *
 * Reset the video/rendering mode for the menu
****************************************************************************/
void
ResetVideo_Menu ()
{
	#ifdef HW_RVL
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9 && (*(u32*)(0xCD8005A0) >> 16) == 0xCAFE) // Wii U
	{
		/* vWii widescreen patch by tueidj */
		write32(0xd8006a0, 0x30000004), mask32(0xd8006a8, 0, 2);
	}
	#endif
	
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

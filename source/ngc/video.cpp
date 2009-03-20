/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 * softdev 2007
 *
 * video.cpp
 *
 * Generic GX Support for Emulators
 * NGC GX Video Functions
 * These are pretty standard functions to setup and use GX scaling.
 ***************************************************************************/
#include <gccore.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <wiiuse/wpad.h>
#include "images/bg.h"
#include "vba.h"
#include "menudraw.h"
#include "gui/gui.h"

s32 CursorX, CursorY;
bool CursorVisible;
bool CursorValid;
bool TiltScreen = false;
float TiltAngle = 0;
u32 FrameTimer = 0;
GuiImageData * pointer1;

/*** External 2D Video ***/
/*** 2D Video Globals ***/
static GXRModeObj *vmode = NULL; // Graphics Mode Object
unsigned int *xfb[2]; // Framebuffers
int whichfb = 0; // Frame buffer toggle

int screenheight;
int screenwidth;

/*** 3D GX ***/
#define DEFAULT_FIFO_SIZE ( 256 * 1024 )
static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN(32);
static unsigned int copynow = GX_FALSE;

/*** Texture memory ***/
static u8 *texturemem = NULL;
static int texturesize;

static GXTexObj texobj;
static Mtx view;
static int vwidth, vheight;
static int updateScaling;
bool progressive = false;

/* New texture based scaler */
typedef struct tagcamera
  {
    Vector pos;
    Vector up;
    Vector view;
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
#define TSTACK 16384
static lwpq_t videoblankqueue;
static lwp_t vbthread;
static unsigned char vbstack[TSTACK];

/****************************************************************************
 * vbgetback
 *
 * This callback enables the emulator to keep running while waiting for a
 * vertical blank.
 *
 * Putting LWP to good use :)
 ***************************************************************************/
static void *
vbgetback (void *arg)
{
	while (1)
	{
		VIDEO_WaitVSync ();	 /**< Wait for video vertical blank */
		LWP_SuspendThread (vbthread);
	}

	return NULL;
}

/****************************************************************************
 * InitVideoThread
 *
 * libOGC provides a nice wrapper for LWP access.
 * This function sets up a new local queue and attaches the thread to it.
 ***************************************************************************/
void
InitVideoThread ()
{
	/*** Initialise a new queue ***/
	LWP_InitQueue (&videoblankqueue);

	/*** Create the thread on this queue ***/
	LWP_CreateThread (&vbthread, vbgetback, NULL, vbstack, TSTACK, 150);
}

/****************************************************************************
 * copy_to_xfb
 *
 * Stock code to copy the GX buffer to the current display mode.
 * Also increments the frameticker, as it's called for each vb.
 ***************************************************************************/
static void
copy_to_xfb (u32 arg)
{
	if (copynow == GX_TRUE)
	{
		GX_CopyDisp (xfb[whichfb], GX_TRUE);
		GX_Flush ();
		copynow = GX_FALSE;
	}

	FrameTimer++;
}

/****************************************************************************
 * Drawing screen
 ***************************************************************************/
void
clearscreen ()
{
	// PAL is 640x576 NOT 640x480!
	// Fill the bottom of the screen with the background's top? left corner
	int colour = bg[0];

	whichfb ^= 1;
	VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], colour);
	if (vmode->xfbHeight==480)
	{
		memcpy (xfb[whichfb], &bg, 1280 * 480);
	}
	else if (vmode->xfbHeight<480)
	{
		memcpy (xfb[whichfb], &bg, 1280 * vmode->xfbHeight);
	}
	else
	{
		memcpy (xfb[whichfb], &bg, 1280 * 240);
		for (int i=0; i<vmode->xfbHeight-480; i++)
			memcpy (((char *)xfb[whichfb])+1280*(240+i), ((char *)&bg)+1280 * 240, 1280 * 1);

		memcpy (((char *)xfb[whichfb])+1280*(vmode->xfbHeight-240), ((char *)&bg)+1280 * 240, 1280 * 240);
	}
}

void
showscreen ()
{
	VIDEO_SetNextFramebuffer (xfb[whichfb]);
	VIDEO_Flush ();
	updateRumbleFrame();
	VIDEO_WaitVSync ();
}

/****************************************************************************
 * Scaler Support Functions
 ****************************************************************************/
static void draw_init(void)
{
	GX_ClearVtxDesc ();
	GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_CLR0, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

	GX_SetNumTexGens (1);
	GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_InvVtxCache ();	// update vertex cache

	GX_InitTexObj(&texobj, texturemem, vwidth, vheight, GX_TF_RGB565,
		GX_CLAMP, GX_CLAMP, GX_FALSE);
	if (!(GCSettings.render&1))
		GX_InitTexObjLOD(&texobj,GX_NEAR,GX_NEAR_MIP_NEAR,2.5,9.0,0.0,GX_FALSE,GX_FALSE,GX_ANISO_1); // original/unfiltered video mode: force texture filtering OFF
}

static void draw_vert(u8 pos, u8 c, f32 s, f32 t)
{
	GX_Position1x8(pos);
	GX_Color1x8(c);
	GX_TexCoord2f32(s, t);
}

static void draw_square(Mtx v)
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
	draw_vert(0, 0, 0.0, 0.0);
	draw_vert(1, 0, 1.0, 0.0);
	draw_vert(2, 0, 1.0, 1.0);
	draw_vert(3, 0, 0.0, 1.0);
	GX_End();
}

void Menu_DrawImg(f32 xpos, f32 ypos, u16 width, u16 height, u8 data[], f32 degrees, f32 scaleX, f32 scaleY, u8 alpha);

static void draw_cursor(Mtx v)
{
	if (!CursorVisible)
		return;
/*
#ifdef HW_RVL
	WPADData *wp = WPAD_Data(0);

	if(wp->ir.valid)
		Menu_DrawImg(wp->ir.x-48, wp->ir.y-48, 96, 96, pointer1->GetImage(), wp->ir.angle, 1, 1, 255);
#endif
*/

	Mtx m;			// model matrix.

	guMtxIdentity(m);
	guMtxScaleApply(m, m, 0.05f, 0.05f, 0.06f);
	guMtxTransApply(m, m, CursorX-320, 240-CursorY, -100);

	GX_LoadPosMtxImm(m, GX_PNMTX0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	draw_vert(0, 0, 0.0, 0.0);
	draw_vert(1, 0, 1.0, 0.0);
	draw_vert(2, 0, 1.0, 1.0);
	draw_vert(3, 0, 0.0, 1.0);
	GX_End();
}

/****************************************************************************
 * StartGX
 ****************************************************************************/
static void GX_Start()
{
	Mtx44 p;

	GXColor background = { 0, 0, 0, 0xff };

	/*** Clear out FIFO area ***/
	memset (gp_fifo, 0, DEFAULT_FIFO_SIZE);

	/*** Initialise GX ***/
	GX_Init (&gp_fifo, DEFAULT_FIFO_SIZE);
	GX_SetCopyClear (background, 0x00ffffff);

	GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale ((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
	GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);

	GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopyDst (vmode->fbWidth, vmode->xfbHeight);
	GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);

	GX_SetFieldMode (vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

	GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetCullMode (GX_CULL_NONE);
	GX_SetDispCopyGamma (GX_GM_1_0);

	guOrtho(p, 480/2, -(480/2), -(640/2), 640/2, 10, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

	GX_CopyDisp (xfb[whichfb], GX_TRUE); // reset xfb
	GX_Flush();

	pointer1 = new GuiImageData(player1_point_png);
}

/****************************************************************************
 * UpdatePadsCB
 *
 * called by postRetraceCallback in InitGCVideo - scans gcpad and wpad
 ***************************************************************************/
static void
UpdatePadsCB ()
{
#ifdef HW_RVL
	WPAD_ScanPads();
#endif
	PAD_ScanPads();
}

/****************************************************************************
* Initialise Video
*
* Before doing anything in libogc, it's recommended to configure a video
* output.
****************************************************************************/
void InitialiseVideo ()
{
	vmode = VIDEO_GetPreferredMode(NULL);

	switch (vmode->viTVMode >> 2)
	{
		case VI_PAL:
			// 576 lines (PAL 50hz)
			break;

		case VI_NTSC:
			// 480 lines (NTSC 60hz)
			break;

		default:
			// 480 lines (PAL 60Hz)
			break;
	}

#ifdef HW_DOL
/* we have component cables, why don't we switch into progressive?
 * on the Wii, the user can do this themselves on their Wii Settings */
	if(VIDEO_HaveComponentCable())
		vmode = &TVNtsc480Prog;
#endif

	// check for progressive scan
	if (vmode->viTVMode == VI_TVMODE_NTSC_PROG)
		progressive = true;

#ifdef HW_RVL
	// widescreen fix
	if(CONF_GetAspectRatio())
	{
		vmode->viWidth = 678;
		vmode->viXOrigin = (VI_MAX_WIDTH_PAL - 678) / 2;
	}
#endif

	VIDEO_Configure(vmode);

	screenheight = vmode->xfbHeight;
	screenwidth = vmode->fbWidth;

	xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
	xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));

	// A console is always useful while debugging
	console_init (xfb[0], 20, 64, vmode->fbWidth, vmode->xfbHeight, vmode->fbWidth * 2);

	// Clear framebuffers etc.
	VIDEO_ClearFrameBuffer (vmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer (vmode, xfb[1], COLOR_BLACK);
	VIDEO_SetNextFramebuffer (xfb[0]);

	// video callbacks
	VIDEO_SetPostRetraceCallback ((VIRetraceCallback)UpdatePadsCB);
	VIDEO_SetPreRetraceCallback ((VIRetraceCallback)copy_to_xfb);

	VIDEO_SetBlack(FALSE);

	VIDEO_Flush();
	VIDEO_WaitVSync();

	if(vmode->viTVMode&VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	copynow = GX_FALSE;
	GX_Start();
	draw_init();

	InitVideoThread ();
}

static void UpdateScaling()
{
	int xscale;
	int yscale;

	float TvAspectRatio;
	float GameboyAspectRatio;
	float MaxStretchRatio = 1.6f;

	if (GCSettings.scaling == 1)
		MaxStretchRatio = 1.3f;
	else if (GCSettings.scaling == 2)
		MaxStretchRatio = 1.6f;
	else
		MaxStretchRatio = 1.0f;

	#ifdef HW_RVL
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
		TvAspectRatio = 16.0f/9.0f;
	else
		TvAspectRatio = 4.0f/3.0f;
	#else
	if (GCSettings.scaling == 3)
		TvAspectRatio = 16.0f/9.0f;
	else
		TvAspectRatio = 4.0f/3.0f;
	#endif

	if (vwidth == 240) // GBA
		GameboyAspectRatio = 240.0f/160.0f; // assumes square pixels on GB Advance
	else // GB or GBC
		GameboyAspectRatio = 160.0f/144.0f; // assumes square pixels on GB Colour

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
	xscale *= GCSettings.ZoomLevel;
	yscale *= GCSettings.ZoomLevel;

	// Set new aspect
	square[0] = square[9]  = -xscale + GCSettings.xshift;
	square[3] = square[6]  =  xscale + GCSettings.xshift;
	square[1] = square[4]  =  yscale - GCSettings.yshift;
	square[7] = square[10] = -yscale - GCSettings.yshift;
	DCFlushRange (square, 32); // update memory BEFORE the GPU accesses it!

	draw_init ();

	memset(&view, 0, sizeof(Mtx));
	guLookAt(view, &cam.pos, &cam.up, &cam.view);
	GX_SetViewport(0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);

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
	GXRModeObj *rmode;
	Mtx44 p;

	rmode = vmode; // same mode as menu

	// reconfigure VI
	VIDEO_Configure (rmode);
	VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], COLOR_BLACK);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	else
		while (VIDEO_GetNextField())
			VIDEO_WaitVSync();

	// reconfigure GX
	GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale ((f32) rmode->xfbHeight / (f32) rmode->efbHeight);
	GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);

	GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst (rmode->fbWidth, rmode->xfbHeight);
	GX_SetCopyFilter (rmode->aa, rmode->sample_pattern, (GCSettings.render == 1) ? GX_TRUE : GX_FALSE, rmode->vfilter);	// deflickering filter only for filtered mode

	GX_SetFieldMode (rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

	GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetCullMode (GX_CULL_NONE);
	GX_SetDispCopyGamma (GX_GM_1_0);

	guOrtho(p, 480/2, -(480/2), -(640/2), 640/2, 10, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

	// reinitialize texture
	GX_InvalidateTexAll ();
	GX_InitTexObj (&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);	// initialize the texture obj we are going to use
	if (!(GCSettings.render&1))
		GX_InitTexObjLOD(&texobj,GX_NEAR,GX_NEAR_MIP_NEAR,2.5,9.0,0.0,GX_FALSE,GX_FALSE,GX_ANISO_1); // original/unfiltered video mode: force texture filtering OFF

	GX_Flush();

	// set aspect ratio
	updateScaling = 1;
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

	VIDEO_Configure (vmode);
	VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], COLOR_BLACK);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	else
		while (VIDEO_GetNextField())
			VIDEO_WaitVSync();

	GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale ((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
	GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);

	GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopyDst (vmode->fbWidth, vmode->xfbHeight);
	GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);

	GX_SetFieldMode (vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	guOrtho(p, 480/2, -(480/2), -(640/2), 640/2, 10, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);
}

void GX_Render_Init(int width, int height)
{
	if (texturemem)
		free(texturemem);

	/*** Allocate 32byte aligned texture memory ***/
	texturesize = (width * height) * 2;

	texturemem = (u8 *) memalign(32, texturesize);

	memset(texturemem, 0, texturesize);

	/*** Setup for first call to scaler ***/
	vwidth = width;
	vheight = height;
}

/****************************************************************************
* GX_Render
*
* Pass in a buffer, width and height to update as a tiled RGB565 texture
****************************************************************************/
void GX_Render(int width, int height, u8 * buffer, int pitch)
{
	int h, w;
	long long int *dst = (long long int *) texturemem;
	long long int *src1 = (long long int *) buffer;
	long long int *src2 = (long long int *) (buffer + pitch);
	long long int *src3 = (long long int *) (buffer + (pitch * 2));
	long long int *src4 = (long long int *) (buffer + (pitch * 3));
	int rowpitch = (pitch >> 3) * 3;
	int rowadjust = ( pitch % 8 ) * 4;
	char *ra = NULL;

	vwidth = width;
	vheight = height;

	// Ensure previous vb has complete
	while ((LWP_ThreadIsSuspended (vbthread) == 0) || (copynow == GX_TRUE))
		usleep (50);

	whichfb ^= 1;

	if(updateScaling)
		UpdateScaling();

	// clear texture objects
	GX_InvVtxCache();
	GX_InvalidateTexAll();
	GX_SetTevOp(GX_TEVSTAGE0, GX_DECAL);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

	for (h = 0; h < vheight; h += 4)
	{
		for (w = 0; w < (vwidth >> 2); w++)
		{
			*dst++ = *src1++;
			*dst++ = *src2++;
			*dst++ = *src3++;
			*dst++ = *src4++;
		}

		src1 += rowpitch;
		src2 += rowpitch;
		src3 += rowpitch;
		src4 += rowpitch;

		if ( rowadjust )
		{
			ra = (char *)src1;
			src1 = (long long int *)(ra + rowadjust);
			ra = (char *)src2;
			src2 = (long long int *)(ra + rowadjust);
			ra = (char *)src3;
			src3 = (long long int *)(ra + rowadjust);
			ra = (char *)src4;
			src4 = (long long int *)(ra + rowadjust);
		}
	}

	// load texture into GX
	DCFlushRange(texturemem, texturesize);

	GX_SetNumChans(1);
	GX_LoadTexObj(&texobj, GX_TEXMAP0);

	draw_square(view); // render textured quad
	draw_cursor(view); // render cursor
	GX_DrawDone();

	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);

	// EFB is ready to be copied into XFB
	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();
	copynow = GX_TRUE;

	// Return to caller, don't waste time waiting for vb
	LWP_ResumeThread (vbthread);
}

/****************************************************************************
 * Zoom Functions
 ***************************************************************************/
void
zoom (float speed)
{
	if (GCSettings.ZoomLevel > 1)
		GCSettings.ZoomLevel += (speed / -100.0);
	else
		GCSettings.ZoomLevel += (speed / -200.0);

	if (GCSettings.ZoomLevel < 0.5)
		GCSettings.ZoomLevel = 0.5;
	else if (GCSettings.ZoomLevel > 2.0)
		GCSettings.ZoomLevel = 2.0;

	updateScaling = 1;	// update video
}

void
zoom_reset ()
{
	GCSettings.ZoomLevel = 1.0;
	updateScaling = 1;	// update video
}

void Menu_DrawImg(f32 xpos, f32 ypos, u16 width, u16 height, u8 data[], f32 degrees, f32 scaleX, f32 scaleY, u8 alpha)
{
	if(data == NULL)
		return;

	GXTexObj texObj;

	GX_InitTexObj(&texObj, data, width,height, GX_TF_RGBA8,GX_CLAMP, GX_CLAMP,GX_FALSE);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);

	GX_SetTevOp (GX_TEVSTAGE0, GX_MODULATE);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	Mtx m,m1,m2, mv;
	width *=.5;
	height*=.5;
	guMtxIdentity (m1);
	guMtxScaleApply(m1,m1,scaleX,scaleY,1.0);
	Vector axis = (Vector) {0 , 0, 1 };
	guMtxRotAxisDeg (m2, &axis, degrees);
	guMtxConcat(m2,m1,m);

	guMtxTransApply(m,m, xpos+width,ypos+height,0);
	guMtxConcat (view, m, mv);
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
	GX_LoadPosMtxImm (view, GX_PNMTX0);

	GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetVtxDesc (GX_VA_TEX0, GX_NONE);
}

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <wiiuse/wpad.h>
#include "images/bg.h"
#include "pal60.h"

extern unsigned int SMBTimer; // timer to reset SMB connection

/*** External 2D Video ***/
/*** 2D Video Globals ***/
GXRModeObj *vmode = NULL; // Graphics Mode Object
unsigned int *xfb[2]; // Framebuffers
int whichfb = 0; // Frame buffer toggle

int screenheight;

/*** 3D GX ***/
#define DEFAULT_FIFO_SIZE ( 256 * 1024 )
static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN(32);

/*** Texture memory ***/
static u8 *texturemem = NULL;
static int texturesize;

GXTexObj texobj;
static Mtx view;
static int vwidth, vheight, oldvwidth, oldvheight;
static int video_vaspect, video_haspect;
float zoom_level = 1;

#define HASPECT 80
#define VASPECT 45

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
	 Think of the output as a -80 x 80 by -60 x 60 graph.
***/
static s16 square[] ATTRIBUTE_ALIGN(32) = {
      /*
       * X,   Y,  Z
       * Values set are for roughly 4:3 aspect
       */
      -HASPECT, VASPECT, 0,	// 0
      HASPECT, VASPECT, 0,	// 1
      HASPECT, -VASPECT, 0,	// 2
      -HASPECT, -VASPECT, 0,	// 3
    };

static camera cam = { {0.0F, 0.0F, 0.0F},
                      {0.0F, 0.5F, 0.0F},
                      {0.0F, 0.0F, -0.5F}
                    };

/****************************************************************************
 * Drawing screen
 ***************************************************************************/
void
clearscreen ()
{
	int colour = COLOR_BLACK;

	whichfb ^= 1;
	VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], colour);
	memcpy (xfb[whichfb], &bg, 1280 * 480);
}

void
showscreen ()
{
	VIDEO_SetNextFramebuffer (xfb[whichfb]);
	VIDEO_Flush ();
	VIDEO_WaitVSync ();
}

/****************************************************************************
 * StartGX
 ****************************************************************************/
void GX_Start()
{
	Mtx p;

	GXColor gxbackground = { 0, 0, 0, 0xff };

	/*** Clear out FIFO area ***/
	memset(&gp_fifo, 0, DEFAULT_FIFO_SIZE);

	/*** Initialise GX ***/
	GX_Init(&gp_fifo, DEFAULT_FIFO_SIZE);
	GX_SetCopyClear(gxbackground, 0x00ffffff);

	GX_SetViewport(0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
	GX_SetScissor(0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopySrc(0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopyDst(vmode->fbWidth, vmode->xfbHeight);
	GX_SetCopyFilter(vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);
	GX_SetFieldMode(vmode->field_rendering,
				  ((vmode->viHeight ==
					2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(xfb[whichfb ^ 1], GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	guPerspective(p, 60, 1.33F, 10.0F, 1000.0F);
	GX_LoadProjectionMtx(p, GX_PERSPECTIVE);
}

/****************************************************************************
 * UpdatePadsCB
 *
 * called by postRetraceCallback in InitGCVideo - scans gcpad and wpad
 ***************************************************************************/
void
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
    /*** Start VIDEO Subsystem ***/
    VIDEO_Init();

    vmode = VIDEO_GetPreferredMode(NULL);
    VIDEO_Configure(vmode);

    screenheight = vmode->xfbHeight;

    xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
    xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));

    VIDEO_SetNextFramebuffer(xfb[0]);
    VIDEO_SetBlack(FALSE);

    // set timings in VI to PAL60
	/*u32 *vreg = (u32 *)0xCC002000;
	for (int i = 0; i < 64; i++ )
		vreg[i] = vpal60[i];*/

    VIDEO_Flush();
    VIDEO_WaitVSync();

    if(vmode->viTVMode&VI_NON_INTERLACE)
    	VIDEO_WaitVSync();
    VIDEO_SetPostRetraceCallback((VIRetraceCallback)UpdatePadsCB);
    VIDEO_SetNextFramebuffer(xfb[0]);

    GX_Start();
    clearscreen();
}

/****************************************************************************
 * Scaler Support Functions
 ****************************************************************************/
static void draw_init(void)
{
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetArray(GX_VA_POS, square, 3 * sizeof(s16));

	GX_SetNumTexGens(1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_InvalidateTexAll();

	GX_InitTexObj(&texobj, texturemem, vwidth, vheight, GX_TF_RGB565,
				GX_CLAMP, GX_CLAMP, GX_FALSE);
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

	guMtxIdentity(m);
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

void GX_Render_Init(int width, int height, int haspect, int vaspect)
{
	/*** Allocate 32byte aligned texture memory ***/
	texturesize = (width * height) * 2;

	if (texturemem)
		free(texturemem);

	texturemem = (u8 *) memalign(32, texturesize);

	memset(texturemem, 0, texturesize);

	/*** Setup for first call to scaler ***/
	vwidth = vheight = oldvwidth = oldvheight = -1;
	video_vaspect = vaspect;
	video_haspect = haspect;
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

	whichfb ^= 1;

	if ((oldvheight != vheight) || (oldvwidth != vwidth))
	{
		// Update scaling
		int xscale = video_haspect;
		int yscale = video_vaspect;

		// change zoom
		xscale *= zoom_level;
		yscale *= zoom_level;

		// Set new aspect (now with crap AR hack!)
		square[0] = square[9] = (-xscale - 7);
		square[3] = square[6] = (xscale + 7);
		square[1] = square[4] = (yscale + 7);
		square[7] = square[10] = (-yscale - 7);

		GX_InvVtxCache ();	// update vertex cache

		oldvwidth = vwidth;
		oldvheight = vheight;
		draw_init();
		memset(&view, 0, sizeof(Mtx));
		guLookAt(view, &cam.pos, &cam.up, &cam.view);
		GX_SetViewport(0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	}

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

	DCFlushRange(texturemem, texturesize);

	GX_SetNumChans(1);
	GX_LoadTexObj(&texobj, GX_TEXMAP0);

	draw_square(view);

	GX_DrawDone();

	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);
	GX_CopyDisp(xfb[whichfb], GX_TRUE);
	GX_Flush();

	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();

	SMBTimer++;
}

/****************************************************************************
 * Zoom Functions
 ***************************************************************************/
void
zoom (float speed)
{
	if (zoom_level > 1)
		zoom_level += (speed / -100.0);
	else
		zoom_level += (speed / -200.0);

	if (zoom_level < 0.5) zoom_level = 0.5;
	else if (zoom_level > 10.0) zoom_level = 10.0;

	oldvheight = 0;	// update video
}

void
zoom_reset ()
{
	zoom_level = 1.0;

	oldvheight = 0;	// update video
}

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
//#include "pal60.h"

extern unsigned int SMBTimer; // timer to reset SMB connection
u32 FrameTimer = 0;

/*** External 2D Video ***/
/*** 2D Video Globals ***/
GXRModeObj *vmode = NULL; // Graphics Mode Object
unsigned int *xfb[2]; // Framebuffers
int whichfb = 0; // Frame buffer toggle

int screenheight;

/*** 3D GX ***/
#define DEFAULT_FIFO_SIZE ( 256 * 1024 )
static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN(32);
unsigned int copynow = GX_FALSE;

/*** Texture memory ***/
static u8 *texturemem = NULL;
static int texturesize;

GXTexObj texobj;
static Mtx view;
static int vwidth, vheight, oldvwidth, oldvheight;
static int video_vaspect, video_haspect;
int updateScaling;
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

#ifdef VIDEO_THREADING
/****************************************************************************
 * VideoThreading
 ***************************************************************************/
#define TSTACK 16384
lwpq_t videoblankqueue;
lwp_t vbthread;
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
	LWP_CreateThread (&vbthread, vbgetback, NULL, vbstack, TSTACK, 80);
}

#endif

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
	SMBTimer++;
}

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
	GX_SetNumChans (0);

	GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_InvalidateTexAll();
	memset (&view, 0, sizeof (Mtx));
	guLookAt(view, &cam.pos, &cam.up, &cam.view);
	GX_LoadPosMtxImm (view, GX_PNMTX0);
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

/****************************************************************************
 * StartGX
 ****************************************************************************/
void GX_Start()
{
	Mtx p;

	GXColor background = { 0, 0, 0, 0xff };

	/*** Clear out FIFO area ***/
	memset (&gp_fifo, 0, DEFAULT_FIFO_SIZE);

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
	GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate (GX_TRUE);

	guPerspective(p, 60, 1.33F, 10.0F, 1000.0F);
	GX_LoadProjectionMtx(p, GX_PERSPECTIVE);

	GX_SetTevOp(GX_TEVSTAGE0, GX_DECAL);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

	//guOrtho(p, vmode->efbHeight/2, -(vmode->efbHeight/2), -(vmode->fbWidth/2), vmode->fbWidth/2, 10, 1000);	// matrix, t, b, l, r, n, f
	//GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

	GX_CopyDisp (xfb[whichfb], GX_TRUE); // reset xfb
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

#ifdef HW_DOL
/* we have component cables, but the preferred mode is interlaced
 * why don't we switch into progressive?
 * on the Wii, the user can do this themselves on their Wii Settings */
	if(VIDEO_HaveComponentCable() && vmode == &TVNtsc480IntDf)
		vmode = &TVNtsc480Prog;
#endif

	VIDEO_Configure(vmode);

	screenheight = vmode->xfbHeight;

	xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
	xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));

	// Clear framebuffers etc.
	VIDEO_ClearFrameBuffer (vmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer (vmode, xfb[1], COLOR_BLACK);
	VIDEO_SetNextFramebuffer (xfb[0]);

	// video callbacks
	VIDEO_SetPostRetraceCallback ((VIRetraceCallback)UpdatePadsCB);
	VIDEO_SetPreRetraceCallback ((VIRetraceCallback)copy_to_xfb);

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

	copynow = GX_FALSE;
	GX_Start();

	#ifdef VIDEO_THREADING
	InitVideoThread ();
	#endif
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
	Mtx p;

	rmode = vmode; // same mode as menu

	VIDEO_Configure (rmode);
	VIDEO_ClearFrameBuffer (rmode, xfb[whichfb], COLOR_BLACK);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
	else while (VIDEO_GetNextField())  VIDEO_WaitVSync();

	GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale ((f32) rmode->xfbHeight / (f32) rmode->efbHeight);
	GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);

	GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst (rmode->fbWidth, rmode->xfbHeight);
	GX_SetCopyFilter (rmode->aa, rmode->sample_pattern, (GCSettings.render == 0) ? GX_TRUE : GX_FALSE, rmode->vfilter);	// AA on only for filtered mode

	GX_SetFieldMode (rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	//guOrtho(p, rmode->efbHeight/2, -(rmode->efbHeight/2), -(rmode->fbWidth/2), rmode->fbWidth/2, 10, 1000);	// matrix, t, b, l, r, n, f
	//GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);
}

/****************************************************************************
 * ResetVideo_Menu
 *
 * Reset the video/rendering mode for the menu
****************************************************************************/
void
ResetVideo_Menu ()
{
	Mtx p;

	VIDEO_Configure (vmode);
	VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], COLOR_BLACK);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
	else while (VIDEO_GetNextField())  VIDEO_WaitVSync();

	GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale ((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
	GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);

	GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopyDst (vmode->fbWidth, vmode->xfbHeight);
	GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);

	GX_SetFieldMode (vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	//guOrtho(p, vmode->efbHeight/2, -(vmode->efbHeight/2), -(vmode->fbWidth/2), vmode->fbWidth/2, 10, 1000);	// matrix, t, b, l, r, n, f
	//GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);
}

void UpdateScaling()
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

	GX_InitTexObj (&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);	// initialize the texture obj we are going to use

	if (GCSettings.render == 1)
		GX_InitTexObjLOD(&texobj,GX_NEAR,GX_NEAR_MIP_NEAR,2.5,9.0,0.0,GX_FALSE,GX_FALSE,GX_ANISO_1); // original/unfiltered video mode: force texture filtering OFF

	GX_LoadTexObj (&texobj, GX_TEXMAP0);	// load texture object so its ready to use

	draw_init();
	updateScaling = 0;
}

void GX_Render_Init(int width, int height, int haspect, int vaspect)
{
	ResetVideo_Emu ();	// reset video to emulator rendering settings

	if (texturemem)
		free(texturemem);

	/*** Allocate 32byte aligned texture memory ***/
	texturesize = (width * height) * 2;

	texturemem = (u8 *) memalign(32, texturesize);

	memset(texturemem, 0, texturesize);

	/*** Setup for first call to scaler ***/
	vwidth = vheight = oldvwidth = oldvheight = -1;
	updateScaling = 1;
	video_vaspect = vaspect;
	video_haspect = haspect;

	UpdateScaling();
}

/****************************************************************************
 * MakeTexture
 *
 * Proper GNU Asm rendition of the above, converted by shagkur. - Thanks!
 ***************************************************************************/
void
MakeTexture (const void *src, void *dst, s32 width, s32 height)
{
  register u32 tmp0 = 0, tmp1 = 0, tmp2 = 0, tmp3 = 0;

  __asm__ __volatile__ ("	srwi		%6,%6,2\n"
			"	srwi		%7,%7,2\n"
			"	subi		%3,%4,4\n"
			"	mr		%4,%3\n"
			"	subi		%4,%4,4\n"
			"2:	mtctr		%6\n"
			"	mr			%0,%5\n"
			//
			"1:	lwz			%1,0(%5)\n"
			"	stwu		%1,8(%4)\n"
			"	lwz			%2,4(%5)\n"
			"	stwu		%2,8(%3)\n"
			"	lwz			%1,1024(%5)\n"
			"	stwu		%1,8(%4)\n"
			"	lwz			%2,1028(%5)\n"
			"	stwu		%2,8(%3)\n"
			"	lwz			%1,2048(%5)\n"
			"	stwu		%1,8(%4)\n"
			"	lwz			%2,2052(%5)\n"
			"	stwu		%2,8(%3)\n"
			"	lwz			%1,3072(%5)\n"
			"	stwu		%1,8(%4)\n"
			"	lwz			%2,3076(%5)\n"
			"	stwu		%2,8(%3)\n"
			"	addi		%5,%5,8\n"
			"	bdnz		1b\n"
			"	addi		%5,%0,4096\n"
			"	subic.		%7,%7,1\n"
			"	bne			2b"
			//              0                        1                        2                        3              4                      5                 6               7
			:"=&r" (tmp0), "=&r" (tmp1), "=&r" (tmp2),
			"=&r" (tmp3), "+r" (dst):"r" (src), "r" (width),
			"r" (height));
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

	#ifdef VIDEO_THREADING
	// Ensure previous vb has complete
	while ((LWP_ThreadIsSuspended (vbthread) == 0) || (copynow == GX_TRUE))
	#else
	while (copynow == GX_TRUE)
	#endif
	{
	  usleep (50);
	}

	whichfb ^= 1;

	if(updateScaling)
	{
		UpdateScaling();
	}

	if ((oldvheight != vheight) || (oldvwidth != vwidth))
	{
		oldvwidth = vwidth;
		oldvheight = vheight;
		updateScaling = 1;
	}

	//MakeTexture ((char *) buffer, (char *) texturemem, vwidth, vheight);	// convert image to texture

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
	GX_InvalidateTexAll ();

	draw_square(view);

	GX_DrawDone();

	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();
	copynow = GX_TRUE;

#ifdef VIDEO_THREADING
	// Return to caller, don't waste time waiting for vb
	LWP_ResumeThread (vbthread);
#endif
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

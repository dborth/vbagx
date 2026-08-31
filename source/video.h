/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 * softdev 2007
 *
 * video.h
 *
 * Video routines
 ***************************************************************************/

#ifndef _GCVIDEOH_
#define _GCVIDEOH_

#include <ogcsys.h>
#include "libgui/Gui.h"

#define TEX_WIDTH 640
#define TEX_HEIGHT 480
#define TEXTUREMEM_SIZE 	TEX_WIDTH*TEX_HEIGHT*2

void InitializeVideo ();
void GX_Render_Init(int width, int height);
void GX_Render(int gbWidth, int gbHeight, u8 * buffer);
void ResetVideo_Emu();
void ResetVideo_Menu();
void TakeScreenshot(u8 * gameTexture);
void ClearScreenshot();
void Menu_Render();
void InitFPSFontData();

extern GXRModeObj *vmode;
extern s32 CursorX, CursorY;
extern bool CursorVisible;
extern bool CursorValid;
extern bool TiltScreen;
extern float TiltAngle;
extern u32 FrameTimer;
extern bool vmode_60hz;

typedef struct
{
	u8 * buffer;
	int size;
	int width;
	int height;
	float scaleX;
	float scaleY;
	int xoffset;
	int yoffset;
} GameScreenPng;

extern u8* texturemem;
extern GameScreenPng gameScreenPng;

#endif

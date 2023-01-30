/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2023
 * softdev 2007
 *
 * video.h
 *
 * Video routines
 ***************************************************************************/

#ifndef _GCVIDEOH_
#define _GCVIDEOH_

#include <ogcsys.h>

void InitializeVideo ();
void GX_Render_Init(int width, int height);
void GX_Render(int gbWidth, int gbHeight, u8 * buffer);
void StopGX();
void ResetVideo_Emu();
void ResetVideo_Menu();
void TakeScreenshot();
void ClearScreenshot();
void Menu_Render();
void Menu_DrawImg(f32 xpos, f32 ypos, u16 width, u16 height, u8 data[], f32 degrees, f32 scaleX, f32 scaleY, u8 alphaF );
void Menu_DrawRectangle(f32 x, f32 y, f32 width, f32 height, GXColor color, u8 filled);

extern GXRModeObj *vmode;
extern int screenheight;
extern int screenwidth;
extern s32 CursorX, CursorY;
extern bool CursorVisible;
extern bool CursorValid;
extern bool TiltScreen;
extern float TiltAngle;
extern u8 * gameScreenTex;
extern u8 * gameScreenPng;
extern int gameScreenPngSize;
extern u32 FrameTimer;

char *AllocAndGetPNGBorderPath(const char* title);
void SaveSGBBorderIfNoneExists(const void* buffer);
extern u16 *InitialBorder;
extern int InitialBorderWidth;
extern int InitialBorderHeight;
extern bool SGBBorderLoadedFromGame;

#endif

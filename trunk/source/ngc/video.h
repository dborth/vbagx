/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
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
void GX_Render(int width, int height, u8 * buffer, int pitch);
void StopGX();
void ResetVideo_Emu();
void zoom (float speed);
void zoom_reset ();

void ResetVideo_Menu();
void TakeScreenshot();
void Menu_Render();
void Menu_DrawImg(f32 xpos, f32 ypos, u16 width, u16 height, u8 data[], f32 degrees, f32 scaleX, f32 scaleY, u8 alphaF );
void Menu_DrawRectangle(f32 x, f32 y, f32 width, f32 height, GXColor color, u8 filled);

extern int screenheight;
extern int screenwidth;
extern s32 CursorX, CursorY;
extern bool CursorVisible;
extern bool CursorValid;
extern bool TiltScreen;
extern float TiltAngle;
extern u8 * gameScreenTex;
extern u8 * gameScreenTex2;
extern u32 FrameTimer;

#endif

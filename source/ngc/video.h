/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 * softdev 2007
 *
 * video.h
 *
 * Generic GX Support for Emulators
 * NGC GX Video Functions
 * These are pretty standard functions to setup and use GX scaling.
 ***************************************************************************/

#ifndef __VIDEOGX_H__
#define __VIDEOGX_H__

void InitialiseVideo ();
void GX_Render_Init(int width, int height);
void GX_Render(int width, int height, u8 * buffer, int pitch);
void clearscreen ();
void showscreen ();
void zoom (float speed);
void zoom_reset ();
void ResetVideo_Menu ();
void ResetVideo_Emu ();

extern int screenheight;
extern int screenwidth;
extern s32 CursorX, CursorY;
extern bool CursorVisible;
extern bool CursorValid;
extern bool TiltScreen;
extern float TiltAngle;

#endif

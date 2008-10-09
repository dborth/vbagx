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

#ifndef __GXHDR__
#define __GXHDR__

void InitialiseVideo ();
void GX_Start();
void GX_Render_Init(int width, int height, int haspect, int vaspect);
void GX_Render(int width, int height, u8 * buffer, int pitch);
void clearscreen ();
void showscreen ();
void zoom (float speed);
void zoom_reset ();

#endif

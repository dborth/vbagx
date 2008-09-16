/****************************************************************************
*   Generic GX Scaler
*   softdev 2007
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License along
*   with this program; if not, write to the Free Software Foundation, Inc.,
*   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
* NGC GX Video Functions
*
* These are pretty standard functions to setup and use GX scaling.
****************************************************************************/
#ifndef __GXHDR__
#define __GXHDR__

void InitialiseVideo ();
void GX_Render_Init(int width, int height, int haspect, int vaspect);
void GX_Render(int width, int height, u8 * buffer, int pitch);
void clearscreen (int colour = COLOR_BLACK);
void showscreen ();

#endif

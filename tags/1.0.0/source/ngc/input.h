/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * input.h
 *
 * Wii/Gamecube controller management
 ***************************************************************************/

#ifndef _INPUT_H_
#define _INPUT_H_

#include <gccore.h>

#define PI 				3.14159265f
#define PADCAL			50

extern unsigned int gcpadmap[];
extern unsigned int wmpadmap[];
extern unsigned int ccpadmap[];
extern unsigned int ncpadmap[];

s8 WPAD_StickX(u8 chan,u8 right);
s8 WPAD_StickY(u8 chan, u8 right);

u32 GetJoy();

#endif

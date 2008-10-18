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
#define MAXJP 			10

extern unsigned int gcpadmap[];
extern unsigned int wmpadmap[];
extern unsigned int ccpadmap[];
extern unsigned int ncpadmap[];

s8 WPAD_Stick(u8 chan,u8 right, int axis);

u32 GetJoy(int which);

#endif

/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric August 2009
 *
 * wiiusbsupport.h
 *
 * Wii USB Keyboard and USB Mouse support
 ***************************************************************************/

#ifndef _WIIUSBSUPPORT_H_
#define _WIIUSBSUPPORT_H_

#include <wiikeyboard/wsksymdef.h>

#define MOUSEL 	65533
#define MOUSER 	65534
#define MOUSEM 	65535

void StartWiiKeyboardMouse();
bool AnyKeyDown();

extern u16 DownUsbKeys[65536];
extern u16 DownUsbShiftKeys;

#endif

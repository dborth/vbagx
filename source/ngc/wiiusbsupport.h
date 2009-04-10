/****************************************************************************
 * Visual Boy Advance GX
 *
 * Carl Kenner Febuary 2009
 *
 * wiiusbsupport.h
 *
 * Wii USB Keyboard and USB Mouse support
 ***************************************************************************/

#ifndef _WIIUSBSUPPORT_H_
#define _WIIUSBSUPPORT_H_

#define KB_A 		4
#define KB_B 		5
#define KB_C 		6
#define KB_D 		7
#define KB_E 		8
#define KB_F 		9
#define KB_Q 		20
#define KB_R 		21
#define KB_S 		22
#define KB_V 		25
#define KB_W 		26
#define KB_X 		27
#define KB_Y 		28
#define KB_Z 		29
#define KB_ENTER 	40
#define KB_ESC 		41
#define KB_BKSP 	42
#define KB_TAB 		43
#define KB_SPACE 	44
#define KB_F1		58
#define KB_F2 		59
#define KB_F3 		60
#define KB_F4 		61
#define KB_F5 		62
#define KB_F6 		63
#define KB_F7 		64
#define KB_F8 		65
#define KB_F9 		66
#define KB_F10 		67
#define KB_F11 		68
#define KB_F12 		69
#define KB_PRTSC 	70
#define KB_SCRLK 	71
#define KB_PAUSE 	72
#define KB_RIGHT 	79
#define KB_LEFT 	80
#define KB_DOWN 	81
#define KB_UP 		82
#define KB_LCTRL 	224
#define KB_LSHIFT 	225
#define KB_LALT 	226
#define KB_LWIN 	227
#define KB_RCTRL 	228
#define KB_RSHIFT 	229
#define KB_RALT 	230
#define KB_RWIN 	231
// CAKTODO
#define KB_MOUSEL 	232
#define KB_MOUSER 	233
#define KB_MOUSEM 	234

void StartWiiKeyboardMouse();
void StopWiiKeyboard();
bool AnyKeyDown();
void StartWiiMouse();

extern u8 DownUsbKeys[256];
extern u8 DownUsbShiftKeys;

#endif

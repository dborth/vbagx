/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric August 2009
 *
 * wiiusbsupport.cpp
 *
 * Wii USB Keyboard and USB Mouse support
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <ogc/ipc.h>
#include <wiiuse/wpad.h>
#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <ogc/usbmouse.h>
#include <wiikeyboard/keyboard.h>

#include "wiiusbsupport.h"

u16 DownUsbKeys[65536];

#ifdef HW_RVL

static lwp_t eventthread = LWP_THREAD_NULL;

u16 DownUsbShiftKeys = 0;

s32 MouseDirectInputX = 0;
s32 MouseDirectInputY = 0;

static void KeyPress(u16 key)
{
	if(key >= 0x61 && key <= 0x7a) // lowercase letter
		DownUsbKeys[key-32] = 1; // press the uppercase letter too
	else if(key >= 0x41 && key <= 0x5a) // uppercase letter
		DownUsbKeys[key+32] = 1; // press the lowercase letter too

	DownUsbKeys[key] = 1;
}

static void KeyRelease(u16 key)
{
	if(key >= 0x61 && key <= 0x7a) // lowercase letter
		DownUsbKeys[key-32] = 0; // unpress the uppercase letter too
	else if(key >= 0x41 && key <= 0x5a) // uppercase letter
		DownUsbKeys[key+32] = 0; // unpress the lowercase letter too

	DownUsbKeys[key] = 0;
}

static void * eventcallback(void *arg)
{
	s32 stat;
	s32 mstat;
	keyboard_event ke;
	mouse_event me;

	while(1)
	{
		stat = KEYBOARD_GetEvent(&ke);
		mstat = MOUSE_GetEvent(&me);

		if (stat)
		{
			if(ke.type == KEYBOARD_PRESSED)
				KeyPress(ke.symbol);
			else if(ke.type == KEYBOARD_RELEASED)
				KeyRelease(ke.symbol);
		}
		if (mstat)
		{
			MouseDirectInputX += me.rx;
			MouseDirectInputY += me.ry;
			DownUsbKeys[MOUSEL] = (me.button & 1);
			DownUsbKeys[MOUSER] = (me.button & 2);
			DownUsbKeys[MOUSEM] = (me.button & 4);
		}
		usleep(10000);
	}
	return NULL;
}

bool AnyKeyDown()
{
	for (int i=0; i<65536; i++)
	{
		if (DownUsbKeys[i])
			return true;
	}
	return false;
}

void StartWiiKeyboardMouse()
{
	memset(DownUsbKeys, 0, sizeof(DownUsbKeys));

	MOUSE_Init();
	KEYBOARD_Init(NULL);
	if(eventthread == LWP_THREAD_NULL)
		LWP_CreateThread(&eventthread, eventcallback, NULL, NULL, 0, 80);
}

#else // GameCube stub

bool AnyKeyDown()
{
	return false;
}
void StartWiiKeyboardMouse()
{
	memset(DownUsbKeys, 0, sizeof(DownUsbKeys));
}

#endif

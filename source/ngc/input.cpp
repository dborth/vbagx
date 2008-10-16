/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * input.cpp
 *
 * Wii/Gamecube controller management
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <ogcsys.h>
#include <unistd.h>
#include <wiiuse/wpad.h>

#include "vba.h"
#include "vbasupport.h"
#include "button_mapping.h"
#include "menu.h"
#include "video.h"
#include "input.h"
#include "tbtime.h"

#define VBA_BUTTON_A		1
#define VBA_BUTTON_B		2
#define VBA_BUTTON_SELECT	4
#define VBA_BUTTON_START	8
#define VBA_RIGHT			16
#define VBA_LEFT			32
#define VBA_UP				64
#define VBA_DOWN			128
#define VBA_BUTTON_R		256
#define VBA_BUTTON_L		512
#define VBA_SPEED			1024
#define VBA_CAPTURE			2048

// VBA controller buttons
// All other pads are mapped to this
unsigned int vbapadmap[] = {
	VBA_BUTTON_B, VBA_BUTTON_A,
	VBA_BUTTON_SELECT, VBA_BUTTON_START,
	VBA_UP, VBA_DOWN,
	VBA_LEFT, VBA_RIGHT,
	VBA_BUTTON_L, VBA_BUTTON_R
};

/*** Gamecube controller Padmap ***/
unsigned int gcpadmap[] = {
  PAD_BUTTON_B, PAD_BUTTON_A,
  PAD_TRIGGER_Z, PAD_BUTTON_START,
  PAD_BUTTON_UP, PAD_BUTTON_DOWN,
  PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT,
  PAD_TRIGGER_L, PAD_TRIGGER_R
};
/*** Wiimote Padmap ***/
unsigned int wmpadmap[] = {
  WPAD_BUTTON_1, WPAD_BUTTON_2,
  WPAD_BUTTON_MINUS, WPAD_BUTTON_PLUS,
  WPAD_BUTTON_RIGHT, WPAD_BUTTON_LEFT,
  WPAD_BUTTON_UP, WPAD_BUTTON_DOWN,
  WPAD_BUTTON_B, WPAD_BUTTON_A
};
/*** Classic Controller Padmap ***/
unsigned int ccpadmap[] = {
  WPAD_CLASSIC_BUTTON_Y, WPAD_CLASSIC_BUTTON_B,
  WPAD_CLASSIC_BUTTON_MINUS, WPAD_CLASSIC_BUTTON_PLUS,
  WPAD_CLASSIC_BUTTON_UP, WPAD_CLASSIC_BUTTON_DOWN,
  WPAD_CLASSIC_BUTTON_LEFT, WPAD_CLASSIC_BUTTON_RIGHT,
  WPAD_CLASSIC_BUTTON_FULL_L, WPAD_CLASSIC_BUTTON_FULL_R
};
/*** Nunchuk + wiimote Padmap ***/
unsigned int ncpadmap[] = {
  WPAD_NUNCHUK_BUTTON_C, WPAD_NUNCHUK_BUTTON_Z,
  WPAD_BUTTON_MINUS, WPAD_BUTTON_PLUS,
  WPAD_BUTTON_UP, WPAD_BUTTON_DOWN,
  WPAD_BUTTON_LEFT, WPAD_BUTTON_RIGHT,
  WPAD_BUTTON_2, WPAD_BUTTON_1
};

#ifdef HW_RVL
/****************************************************************************
 * WPAD_StickX
 *
 * Get X value from Wii Joystick (classic, nunchuk) input
 ***************************************************************************/

s8 WPAD_StickX(u8 chan,u8 right)
{
	float mag = 0.0;
	float ang = 0.0;
	WPADData *data = WPAD_Data(chan);

	switch (data->exp.type)
	{
		case WPAD_EXP_NUNCHUK:
		case WPAD_EXP_GUITARHERO3:
			if (right == 0)
			{
				mag = data->exp.nunchuk.js.mag;
				ang = data->exp.nunchuk.js.ang;
			}
			break;

		case WPAD_EXP_CLASSIC:
			if (right == 0)
			{
				mag = data->exp.classic.ljs.mag;
				ang = data->exp.classic.ljs.ang;
			}
			else
			{
				mag = data->exp.classic.rjs.mag;
				ang = data->exp.classic.rjs.ang;
			}
			break;

		default:
			break;
	}

	/* calculate X value (angle need to be converted into radian) */
	if (mag > 1.0) mag = 1.0;
	else if (mag < -1.0) mag = -1.0;
	double val = mag * sin((PI * ang)/180.0f);

	return (s8)(val * 128.0f);
}

/****************************************************************************
 * WPAD_StickY
 *
 * Get Y value from Wii Joystick (classic, nunchuk) input
 ***************************************************************************/

s8 WPAD_StickY(u8 chan, u8 right)
{
	float mag = 0.0;
	float ang = 0.0;
	WPADData *data = WPAD_Data(chan);

	switch (data->exp.type)
	{
		case WPAD_EXP_NUNCHUK:
		case WPAD_EXP_GUITARHERO3:
			if (right == 0)
			{
				mag = data->exp.nunchuk.js.mag;
				ang = data->exp.nunchuk.js.ang;
			}
			break;

			case WPAD_EXP_CLASSIC:
			if (right == 0)
			{
				mag = data->exp.classic.ljs.mag;
				ang = data->exp.classic.ljs.ang;
			}
			else
			{
				mag = data->exp.classic.rjs.mag;
				ang = data->exp.classic.rjs.ang;
			}
			break;

		default:
			break;
	}

	/* calculate X value (angle need to be converted into radian) */
	if (mag > 1.0) mag = 1.0;
	else if (mag < -1.0) mag = -1.0;
	double val = mag * cos((PI * ang)/180.0f);

	return (s8)(val * 128.0f);
}
#endif

/****************************************************************************
 * DecodeJoy
 *
 * Reads the changes (buttons pressed, etc) from a controller and reports
 * these changes to VBA
 ****************************************************************************/

u32 DecodeJoy(unsigned short pad)
{
	signed char pad_x = PAD_StickX (pad);
	signed char pad_y = PAD_StickY (pad);
	signed char gc_px = PAD_SubStickX (0);
	u32 jp = PAD_ButtonsHeld (pad);
	u32 J = 0;

	#ifdef HW_RVL
	signed char wm_ax = WPAD_StickX ((u8)pad, 0);
	signed char wm_ay = WPAD_StickY ((u8)pad, 0);
	u32 wp = WPAD_ButtonsHeld (pad);
	signed char wm_sx = WPAD_StickX (0,1); // CC right joystick

	u32 exp_type;
	if ( WPAD_Probe(pad, &exp_type) != 0 ) exp_type = WPAD_EXP_NONE;
	#endif

	/***
	Gamecube Joystick input
	***/
	// Is XY inside the "zone"?
	if (pad_x * pad_x + pad_y * pad_y > PADCAL * PADCAL)
	{
		if (pad_x > 0 && pad_y == 0) J |= VBA_RIGHT;
		if (pad_x < 0 && pad_y == 0) J |= VBA_LEFT;
		if (pad_x == 0 && pad_y > 0) J |= VBA_UP;
		if (pad_x == 0 && pad_y < 0) J |= VBA_DOWN;

		if (pad_x != 0 && pad_y != 0)
		{
			if ((float)pad_y / pad_x >= -2.41421356237 && (float)pad_y / pad_x < 2.41421356237)
			{
				if (pad_x >= 0)
					J |= VBA_RIGHT;
				else
					J |= VBA_LEFT;
			}

			if ((float)pad_x / pad_y >= -2.41421356237 && (float)pad_x / pad_y < 2.41421356237)
			{
				if (pad_y >= 0)
					J |= VBA_UP;
				else
					J |= VBA_DOWN;
			}
		}
	}
#ifdef HW_RVL
	/***
	Wii Joystick (classic, nunchuk) input
	***/
	// Is XY inside the "zone"?
	if (wm_ax * wm_ax + wm_ay * wm_ay > PADCAL * PADCAL)
	{
		/*** we don't want division by zero ***/
		if (wm_ax > 0 && wm_ay == 0)
			J |= VBA_RIGHT;
		if (wm_ax < 0 && wm_ay == 0)
			J |= VBA_LEFT;
		if (wm_ax == 0 && wm_ay > 0)
			J |= VBA_UP;
		if (wm_ax == 0 && wm_ay < 0)
			J |= VBA_DOWN;

		if (wm_ax != 0 && wm_ay != 0)
		{

			/*** Recalc left / right ***/
			float t;

			t = (float) wm_ay / wm_ax;
			if (t >= -2.41421356237 && t < 2.41421356237)
			{
				if (wm_ax >= 0)
					J |= VBA_RIGHT;
				else
					J |= VBA_LEFT;
			}

			/*** Recalc up / down ***/
			t = (float) wm_ax / wm_ay;
			if (t >= -2.41421356237 && t < 2.41421356237)
			{
				if (wm_ay >= 0)
					J |= VBA_UP;
				else
					J |= VBA_DOWN;
			}
		}
	}
#endif

	// Zoom feature
	if(
	(gc_px > 70)
	#ifdef HW_RVL
	|| (wm_sx > 70)
	|| ((wp & WPAD_BUTTON_A) && (wp & WPAD_BUTTON_B))
	#endif
	)
		J |= VBA_SPEED;

	/*** Report pressed buttons (gamepads) ***/
	int i;

	for (i = 0; i < MAXJP; i++)
	{
		if ( (jp & gcpadmap[i])											// gamecube controller
		#ifdef HW_RVL
		|| ( (exp_type == WPAD_EXP_NONE) && (wp & wmpadmap[i]) )	// wiimote
		|| ( (exp_type == WPAD_EXP_CLASSIC) && (wp & ccpadmap[i]) )	// classic controller
		|| ( (exp_type == WPAD_EXP_NUNCHUK) && (wp & ncpadmap[i]) )	// nunchuk + wiimote
		#endif
		)
			J |= vbapadmap[i];
	}

	if ((J & 48) == 48)
		J &= ~16;
	if ((J & 192) == 192)
		J &= ~128;

	return J;
}
u32 GetJoy()
{
    int pad = 0;

    s8 gc_px = PAD_SubStickX (0);

    #ifdef HW_RVL
    s8 wm_sx = WPAD_StickX (0,1);
    u32 wm_pb = WPAD_ButtonsHeld (0); // wiimote / expansion button info
    #endif

    // request to go back to menu
    if ((gc_px < -70)
    #ifdef HW_RVL
    		 || (wm_pb & WPAD_BUTTON_HOME)
    		 || (wm_pb & WPAD_CLASSIC_BUTTON_HOME)
    		 || (wm_sx < -70)
    #endif
    )
	{
    	if (GCSettings.AutoSave == 1)
    	{
    		SaveBatteryOrState(GCSettings.SaveMethod, 0, SILENT); // save battery
    	}
    	else if (GCSettings.AutoSave == 2)
    	{
			SaveBatteryOrState(GCSettings.SaveMethod, 1, SILENT); // save state
    	}
    	else if(GCSettings.AutoSave == 3)
    	{
    		SaveBatteryOrState(GCSettings.SaveMethod, 0, SILENT); // save battery
    		SaveBatteryOrState(GCSettings.SaveMethod, 1, SILENT); // save state
    	}
    	MainMenu(3);
    	return 0;
	}
	else
	{
		return DecodeJoy(pad);
	}
}

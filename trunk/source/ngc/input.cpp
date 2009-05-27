/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2009
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
#include "button_mapping.h"
#include "audio.h"
#include "video.h"
#include "input.h"
#include "gameinput.h"
#include "vbasupport.h"
#include "wiiusbsupport.h"
#include "gui/gui.h"
#include "gba/GBA.h"
#include "gba/bios.h"
#include "gba/GBAinline.h"

int rumbleRequest[4] = {0,0,0,0};
GuiTrigger userInput[4];

#ifdef HW_RVL
static int rumbleCount[4] = {0,0,0,0};
#endif

bool cartridgeRumble = false;
int gameRumbleCount = 0, menuRumbleCount = 0, rumbleCountAlready = 0;

unsigned int vbapadmap[10]; // VBA controller buttons
u32 btnmap[5][10]; // button mapping

void ResetControls()
{
	memset(btnmap, 0, sizeof(btnmap));

	int i;

	// VBA controller buttons
	// All other pads are mapped to this
	i=0;
	vbapadmap[i++] = VBA_BUTTON_B;
	vbapadmap[i++] = VBA_BUTTON_A;
	vbapadmap[i++] = VBA_BUTTON_SELECT;
	vbapadmap[i++] = VBA_BUTTON_START;
	vbapadmap[i++] = VBA_UP;
	vbapadmap[i++] = VBA_DOWN;
	vbapadmap[i++] = VBA_LEFT;
	vbapadmap[i++] = VBA_RIGHT;
	vbapadmap[i++] = VBA_BUTTON_L;
	vbapadmap[i++] = VBA_BUTTON_R;

	/*** Gamecube controller Padmap ***/
	i=0;
	btnmap[CTRLR_GCPAD][i++] = PAD_BUTTON_B;
	btnmap[CTRLR_GCPAD][i++] = PAD_BUTTON_A;
	btnmap[CTRLR_GCPAD][i++] = PAD_TRIGGER_Z;
	btnmap[CTRLR_GCPAD][i++] = PAD_BUTTON_START;
	btnmap[CTRLR_GCPAD][i++] = PAD_BUTTON_UP;
	btnmap[CTRLR_GCPAD][i++] = PAD_BUTTON_DOWN;
	btnmap[CTRLR_GCPAD][i++] = PAD_BUTTON_LEFT;
	btnmap[CTRLR_GCPAD][i++] = PAD_BUTTON_RIGHT;
	btnmap[CTRLR_GCPAD][i++] = PAD_TRIGGER_L;
	btnmap[CTRLR_GCPAD][i++] = PAD_TRIGGER_R;

	/*** Wiimote Padmap ***/
	i=0;
	btnmap[CTRLR_WIIMOTE][i++] = WPAD_BUTTON_1;
	btnmap[CTRLR_WIIMOTE][i++] = WPAD_BUTTON_2;
	btnmap[CTRLR_WIIMOTE][i++] = WPAD_BUTTON_MINUS;
	btnmap[CTRLR_WIIMOTE][i++] = WPAD_BUTTON_PLUS;
	btnmap[CTRLR_WIIMOTE][i++] = WPAD_BUTTON_RIGHT;
	btnmap[CTRLR_WIIMOTE][i++] = WPAD_BUTTON_LEFT;
	btnmap[CTRLR_WIIMOTE][i++] = WPAD_BUTTON_UP;
	btnmap[CTRLR_WIIMOTE][i++] = WPAD_BUTTON_DOWN;
	btnmap[CTRLR_WIIMOTE][i++] = WPAD_BUTTON_B;
	btnmap[CTRLR_WIIMOTE][i++] = WPAD_BUTTON_A;

	/*** Classic Controller Padmap ***/
	i=0;
	btnmap[CTRLR_CLASSIC][i++] = WPAD_CLASSIC_BUTTON_Y;
	btnmap[CTRLR_CLASSIC][i++] = WPAD_CLASSIC_BUTTON_B;
	btnmap[CTRLR_CLASSIC][i++] = WPAD_CLASSIC_BUTTON_MINUS;
	btnmap[CTRLR_CLASSIC][i++] = WPAD_CLASSIC_BUTTON_PLUS;
	btnmap[CTRLR_CLASSIC][i++] = WPAD_CLASSIC_BUTTON_UP;
	btnmap[CTRLR_CLASSIC][i++] = WPAD_CLASSIC_BUTTON_DOWN;
	btnmap[CTRLR_CLASSIC][i++] = WPAD_CLASSIC_BUTTON_LEFT;
	btnmap[CTRLR_CLASSIC][i++] = WPAD_CLASSIC_BUTTON_RIGHT;
	btnmap[CTRLR_CLASSIC][i++] = WPAD_CLASSIC_BUTTON_FULL_L;
	btnmap[CTRLR_CLASSIC][i++] = WPAD_CLASSIC_BUTTON_FULL_R;

	/*** Nunchuk + wiimote Padmap ***/
	i=0;
	btnmap[CTRLR_NUNCHUK][i++] = WPAD_NUNCHUK_BUTTON_C;
	btnmap[CTRLR_NUNCHUK][i++] = WPAD_NUNCHUK_BUTTON_Z;
	btnmap[CTRLR_NUNCHUK][i++] = WPAD_BUTTON_MINUS;
	btnmap[CTRLR_NUNCHUK][i++] = WPAD_BUTTON_PLUS;
	btnmap[CTRLR_NUNCHUK][i++] = WPAD_BUTTON_UP;
	btnmap[CTRLR_NUNCHUK][i++] = WPAD_BUTTON_DOWN;
	btnmap[CTRLR_NUNCHUK][i++] = WPAD_BUTTON_LEFT;
	btnmap[CTRLR_NUNCHUK][i++] = WPAD_BUTTON_RIGHT;
	btnmap[CTRLR_NUNCHUK][i++] = WPAD_BUTTON_2;
	btnmap[CTRLR_NUNCHUK][i++] = WPAD_BUTTON_1;

	/*** Keyboard map ***/
	i=0;
	btnmap[CTRLR_KEYBOARD][i++] = KB_X; // VBA stupidly has B on the right instead of left
	btnmap[CTRLR_KEYBOARD][i++] = KB_Z;
	btnmap[CTRLR_KEYBOARD][i++] = KB_BKSP;
	btnmap[CTRLR_KEYBOARD][i++] = KB_ENTER;
	btnmap[CTRLR_KEYBOARD][i++] = KB_UP;
	btnmap[CTRLR_KEYBOARD][i++] = KB_DOWN;
	btnmap[CTRLR_KEYBOARD][i++] = KB_LEFT;
	btnmap[CTRLR_KEYBOARD][i++] = KB_RIGHT;
	btnmap[CTRLR_KEYBOARD][i++] = KB_A;
	btnmap[CTRLR_KEYBOARD][i++] = KB_S;
}

#ifdef HW_RVL

/****************************************************************************
 * ShutoffRumble
 ***************************************************************************/

void ShutoffRumble()
{
#ifdef HW_RVL
	for(int i=0;i<4;i++)
	{
		WPAD_Rumble(i, 0);
		rumbleCount[i] = 0;
	}
#endif
	PAD_ControlMotor(PAD_CHAN0, PAD_MOTOR_STOP);
}

/****************************************************************************
 * DoRumble
 ***************************************************************************/

void DoRumble(int i)
{
	if(!GCSettings.Rumble) return;
	if(rumbleRequest[i] && rumbleCount[i] < 3)
	{
		WPAD_Rumble(i, 1); // rumble on
		rumbleCount[i]++;
	}
	else if(rumbleRequest[i])
	{
		rumbleCount[i] = 12;
		rumbleRequest[i] = 0;
	}
	else
	{
		if(rumbleCount[i])
			rumbleCount[i]--;
		WPAD_Rumble(i, 0); // rumble off
	}
}
#endif

static void updateRumble()
{
	bool r = false;
	if (ConfigRequested) r = (menuRumbleCount > 0);
	else r = cartridgeRumble || (gameRumbleCount > 0) || (menuRumbleCount > 0);

#ifdef HW_RVL
	// Rumble wii remote 0
	WPAD_Rumble(0, r);
#endif
	PAD_ControlMotor(PAD_CHAN0, r?PAD_MOTOR_RUMBLE:PAD_MOTOR_STOP);
}

void updateRumbleFrame() {
	// If we already rumbled continuously for more than 50 frames,
	// then disable rumbling for a while.
	if (rumbleCountAlready > 50) {
		gameRumbleCount = 0;
		menuRumbleCount = 0;
		// disable rumbling for 10 more frames
		if (rumbleCountAlready > 50+10)
			rumbleCountAlready = 0;
		else rumbleCountAlready++;
	} else if (ConfigRequested) {
		if (menuRumbleCount>0)
			rumbleCountAlready++;
	} else {
		if (gameRumbleCount>0 || menuRumbleCount>0 || cartridgeRumble)
			rumbleCountAlready++;
	}
	updateRumble();
	if (gameRumbleCount>0 && !ConfigRequested) gameRumbleCount--;
	if (menuRumbleCount>0) menuRumbleCount--;
}

void systemCartridgeRumble(bool RumbleOn) {
	cartridgeRumble = RumbleOn;
	updateRumble();
}

void systemGameRumble(int RumbleForFrames) {
	if (RumbleForFrames > gameRumbleCount) gameRumbleCount = RumbleForFrames;
}

void systemGameRumbleOnlyFor(int OnlyRumbleForFrames) {
	gameRumbleCount = OnlyRumbleForFrames;
}

void systemMenuRumble(int RumbleForFrames) {
	if (RumbleForFrames > menuRumbleCount) menuRumbleCount = RumbleForFrames;
}

/****************************************************************************
 * WPAD_Stick
 *
 * Get X/Y value from Wii Joystick (classic, nunchuk) input
 ***************************************************************************/

s8 WPAD_Stick(u8 chan, u8 right, int axis)
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

	/* calculate x/y value (angle need to be converted into radian) */
	if (mag > 1.0) mag = 1.0;
	else if (mag < -1.0) mag = -1.0;
	double val;

	if(axis == 0) // x-axis
		val = mag * sin((PI * ang)/180.0f);
	else // y-axis
		val = mag * cos((PI * ang)/180.0f);

	return (s8)(val * 128.0f);
}

u32 StandardMovement(unsigned short pad)
{
	u32 J = 0;
	signed char pad_x = PAD_StickX (pad);
	signed char pad_y = PAD_StickY (pad);
	#ifdef HW_RVL
	signed char wm_ax = WPAD_Stick ((u8)pad, 0, 0);
	signed char wm_ay = WPAD_Stick ((u8)pad, 0, 1);
	#endif
	/***
	Gamecube Joystick input, same as normal
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

	// Turbo feature, keyboard or gamecube only
	if(DownUsbKeys[KB_SPACE])
		J |= VBA_SPEED;
	// Capture feature
	if(DownUsbKeys[KB_PRTSC] | DownUsbKeys[KB_F12])
		J |= VBA_CAPTURE;
#endif
	return J;
}

u32 StandardDPad(unsigned short pad)
{
	u32 J = 0;
	u32 jp = PAD_ButtonsHeld(pad);
#ifdef HW_RVL
	u32 exp_type;
	if ( WPAD_Probe(pad, &exp_type) != 0 )
		exp_type = WPAD_EXP_NONE;
	u32 wp = WPAD_ButtonsHeld(pad);
	if (wp & WPAD_BUTTON_RIGHT)
		J |= VBA_RIGHT;
	if (wp & WPAD_BUTTON_LEFT)
		J |= VBA_LEFT;
	if (wp & WPAD_BUTTON_UP)
		J |= VBA_UP;
	if (wp & WPAD_BUTTON_DOWN)
		J |= VBA_DOWN;
	if (exp_type == WPAD_EXP_CLASSIC)
	{
		if (wp & WPAD_CLASSIC_BUTTON_UP)
			J |= VBA_UP;
		if (wp & WPAD_CLASSIC_BUTTON_DOWN)
			J |= VBA_DOWN;
		if (wp & WPAD_CLASSIC_BUTTON_LEFT)
			J |= VBA_LEFT;
		if (wp & WPAD_CLASSIC_BUTTON_RIGHT)
			J |= VBA_RIGHT;
	}
#endif
	if (jp & PAD_BUTTON_UP)
		J |= VBA_UP;
	if (jp & PAD_BUTTON_DOWN)
		J |= VBA_DOWN;
	if (jp & PAD_BUTTON_LEFT)
		J |= VBA_LEFT;
	if (jp & PAD_BUTTON_RIGHT)
		J |= VBA_RIGHT;
	return J;
}

u32 StandardSideways(unsigned short pad)
{
	u32 J = 0;
#ifdef HW_RVL
	u32 wp = WPAD_ButtonsHeld(pad);

	if (wp & WPAD_BUTTON_RIGHT)
		J |= VBA_UP;
	if (wp & WPAD_BUTTON_LEFT)
		J |= VBA_DOWN;
	if (wp & WPAD_BUTTON_UP)
		J |= VBA_LEFT;
	if (wp & WPAD_BUTTON_DOWN)
		J |= VBA_RIGHT;

	if (wp & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	if (wp & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	if (wp & WPAD_BUTTON_1)
		J |= VBA_BUTTON_B;
	if (wp & WPAD_BUTTON_2)
		J |= VBA_BUTTON_A;
	if (cartridgeType == 2)
	{
		if (wp & WPAD_BUTTON_A)
			J |= VBA_BUTTON_R;
		if (wp & WPAD_BUTTON_B)
			J |= VBA_BUTTON_L;
	}
	else
	{
		if (wp & WPAD_BUTTON_B || wp & WPAD_BUTTON_A)
			J |= VBA_SPEED;
	}
#endif
	return J;
}

u32 StandardClassic(unsigned short pad)
{
	u32 J = 0;
#ifdef HW_RVL
	u32 wp = WPAD_ButtonsHeld(pad);

	if (wp & WPAD_CLASSIC_BUTTON_RIGHT)
		J |= VBA_RIGHT;
	if (wp & WPAD_CLASSIC_BUTTON_LEFT)
		J |= VBA_LEFT;
	if (wp & WPAD_CLASSIC_BUTTON_UP)
		J |= VBA_UP;
	if (wp & WPAD_CLASSIC_BUTTON_DOWN)
		J |= VBA_DOWN;

	if (wp & WPAD_CLASSIC_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	if (wp & WPAD_CLASSIC_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	if (wp & WPAD_CLASSIC_BUTTON_FULL_L || wp & WPAD_CLASSIC_BUTTON_ZL)
		J |= VBA_BUTTON_L;
	if (wp & WPAD_CLASSIC_BUTTON_FULL_R || wp & WPAD_CLASSIC_BUTTON_ZR)
		J |= VBA_BUTTON_R;

	if (wp & WPAD_CLASSIC_BUTTON_A)
		J |= VBA_BUTTON_A;
	if (wp & WPAD_CLASSIC_BUTTON_B)
		J |= VBA_BUTTON_B;
	if (wp & WPAD_CLASSIC_BUTTON_Y || wp & WPAD_CLASSIC_BUTTON_X)
		J |= VBA_SPEED;
#endif
	return J;
}

u32 StandardGamecube(unsigned short pad)
{
	u32 J = 0;
	u32 jp = PAD_ButtonsHeld(pad);
	if (jp & PAD_BUTTON_UP)
		J |= VBA_UP;
	if (jp & PAD_BUTTON_DOWN)
		J |= VBA_DOWN;
	if (jp & PAD_BUTTON_LEFT)
		J |= VBA_LEFT;
	if (jp & PAD_BUTTON_RIGHT)
		J |= VBA_RIGHT;
	if (jp & PAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	if (jp & PAD_BUTTON_B)
		J |= VBA_BUTTON_B;
	if (jp & PAD_BUTTON_START)
		J |= VBA_BUTTON_START;
	if (jp & PAD_BUTTON_X)
		J |= VBA_BUTTON_SELECT;
	if (jp & PAD_TRIGGER_L)
		J |= VBA_BUTTON_L;
	if (jp & PAD_TRIGGER_R)
		J |= VBA_BUTTON_R;
	if (jp & PAD_TRIGGER_Z || jp & PAD_BUTTON_Y)
		J |= VBA_SPEED;
	return J;
}

u32 StandardKeyboard(unsigned short pad)
{
	u32 J = 0;
#ifdef HW_RVL
	if (DownUsbKeys[KB_UP])
		J |= VBA_UP;
	if (DownUsbKeys[KB_DOWN])
		J |= VBA_DOWN;
	if (DownUsbKeys[KB_LEFT])
		J |= VBA_LEFT;
	if (DownUsbKeys[KB_RIGHT])
		J |= VBA_RIGHT;
	if (DownUsbKeys[KB_SPACE])
		J |= VBA_SPEED;
	if (DownUsbKeys[KB_F12] || DownUsbKeys[KB_PRTSC])
		J |= VBA_CAPTURE;
	if (DownUsbKeys[KB_X])
		J |= VBA_BUTTON_A;
	if (DownUsbKeys[KB_Z])
		J |= VBA_BUTTON_B;
	if (DownUsbKeys[KB_A])
		J |= VBA_BUTTON_L;
	if (DownUsbKeys[KB_S])
		J |= VBA_BUTTON_R;
	if (DownUsbKeys[KB_ENTER])
		J |= VBA_BUTTON_START;
	if (DownUsbKeys[KB_BKSP])
	J |= VBA_BUTTON_SELECT;
#endif
	return J;
}

u32 DPadWASD(unsigned short pad)
{
	u32 J = 0;
#ifdef HW_RVL
	if (DownUsbKeys[KB_W])
		J |= VBA_UP;
	if (DownUsbKeys[KB_S])
		J |= VBA_DOWN;
	if (DownUsbKeys[KB_A])
		J |= VBA_LEFT;
	if (DownUsbKeys[KB_D])
		J |= VBA_RIGHT;
#endif
	return J;
}

u32 DPadArrowKeys(unsigned short pad)
{
	u32 J = 0;
#ifdef HW_RVL
	if (DownUsbKeys[KB_UP])
		J |= VBA_UP;
	if (DownUsbKeys[KB_DOWN])
		J |= VBA_DOWN;
	if (DownUsbKeys[KB_LEFT])
		J |= VBA_LEFT;
	if (DownUsbKeys[KB_RIGHT])
		J |= VBA_RIGHT;
#endif
	return J;
}

u32 DecodeKeyboard(unsigned short pad)
{
	u32 J = 0;
	#ifdef HW_RVL
	for (int i = 0; i < MAXJP; i++)
	{
		if (DownUsbKeys[btnmap[CTRLR_KEYBOARD][i]]) // keyboard
			J |= vbapadmap[i];
	}
	#endif
	return J;
}

u32 DecodeGamecube(unsigned short pad)
{
	u32 J = 0;
	u32 jp = PAD_ButtonsHeld(pad);
	for (int i = 0; i < MAXJP; i++)
	{
		if (jp & btnmap[CTRLR_GCPAD][i])
			J |= vbapadmap[i];
	}
	return J;
}

u32 DecodeWiimote(unsigned short pad)
{
	u32 J = 0;
	#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);
	for (int i = 0; i < MAXJP; i++)
	{
		if ( (wp->exp.type == WPAD_EXP_NONE) && (wp->btns_h & btnmap[CTRLR_WIIMOTE][i]) )
			J |= vbapadmap[i];
	}
	#endif
	return J;
}

u32 DecodeClassic(unsigned short pad)
{
	u32 J = 0;
	#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);
	for (int i = 0; i < MAXJP; i++)
	{
		if ( (wp->exp.type == WPAD_EXP_CLASSIC) && (wp->btns_h & btnmap[CTRLR_CLASSIC][i]) )
			J |= vbapadmap[i];
	}
	#endif
	return J;
}

u32 DecodeNunchuk(unsigned short pad)
{
	u32 J = 0;
	#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);
	for (int i = 0; i < MAXJP; i++)
	{
		if ( (wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & btnmap[CTRLR_NUNCHUK][i]) )
			J |= vbapadmap[i];
	}
	#endif
	return J;
}

#ifdef HW_RVL
static u32 CCToGC(u32 w) {
	u32 gc = 0;
	if (w & WPAD_CLASSIC_BUTTON_A) gc |= PAD_BUTTON_A;
	if (w & WPAD_CLASSIC_BUTTON_B) gc |= PAD_BUTTON_B;
	if (w & WPAD_CLASSIC_BUTTON_X) gc |= PAD_BUTTON_X;
	if (w & WPAD_CLASSIC_BUTTON_Y) gc |= PAD_BUTTON_Y;
	if (w & WPAD_CLASSIC_BUTTON_PLUS) gc |= PAD_BUTTON_START;
	if (w & WPAD_CLASSIC_BUTTON_MINUS) gc |= 0;
	if (w & (WPAD_CLASSIC_BUTTON_ZL | WPAD_CLASSIC_BUTTON_ZR)) gc |= PAD_TRIGGER_Z;
	if (w & WPAD_CLASSIC_BUTTON_FULL_L) gc |= PAD_TRIGGER_L;
	if (w & WPAD_CLASSIC_BUTTON_FULL_R) gc |= PAD_TRIGGER_R;
	if (w & WPAD_CLASSIC_BUTTON_UP) gc |= PAD_BUTTON_UP;
	if (w & WPAD_CLASSIC_BUTTON_DOWN) gc |= PAD_BUTTON_DOWN;
	if (w & WPAD_CLASSIC_BUTTON_LEFT) gc |= PAD_BUTTON_LEFT;
	if (w & WPAD_CLASSIC_BUTTON_RIGHT) gc |= PAD_BUTTON_RIGHT;
	return gc;
}
#endif

// For developers who don't have gamecube pads but need to test gamecube input
u32 PAD_ButtonsHeldFake(unsigned short pad)
{
	u32 gc = 0;
	#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);
	if (wp->exp.type == WPAD_EXP_CLASSIC) {
		gc = CCToGC(wp->btns_h);
	}
	#endif
	return gc;
}
u32 PAD_ButtonsDownFake(unsigned short pad)
{
	u32 gc = 0;
	#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);
	if (wp->exp.type == WPAD_EXP_CLASSIC) {
		gc = CCToGC(wp->btns_d);
	}
	#endif
	return gc;
}
u32 PAD_ButtonsUpFake(unsigned short pad)
{
	u32 gc = 0;
	#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);
	if (wp->exp.type == WPAD_EXP_CLASSIC) {
		gc = CCToGC(wp->btns_u);
	}
	#endif
	return gc;
}

/****************************************************************************
 * DecodeJoy
 *
 * Reads the STATE (not changes) from a controller and reports
 * this STATE (not changes) to VBA
 ****************************************************************************/

static u32 DecodeJoy(unsigned short pad)
{
	TiltScreen = false;
	CursorVisible = false;

#ifdef HW_RVL
	CursorX = userInput[pad].wpad.ir.x;
	CursorY = userInput[pad].wpad.ir.y;
	CursorValid = userInput[pad].wpad.ir.valid;
#else
	CursorX = CursorY = CursorValid = 0;
#endif

	// check for games that should have special Wii controls
	if (GCSettings.WiiControls)
	switch (RomIdCode & 0xFFFFFF)
	{
		// Zelda
		case ZELDA1:
			return Zelda1Input(pad);
		case ZELDA2:
			return Zelda2Input(pad);
		case ALINKTOTHEPAST:
			return ALinkToThePastInput(pad);
		case LINKSAWAKENING:
			return LinksAwakeningInput(pad);
		case ORACLEOFAGES:
			return OracleOfAgesInput(pad);
		case ORACLEOFSEASONS:
			return OracleOfSeasonsInput(pad);
		case MINISHCAP:
			return MinishCapInput(pad);

		// Metroid
		case METROID0:
			return MetroidZeroInput(pad);
		case METROID1:
			return Metroid1Input(pad);
		case METROID2:
			return Metroid2Input(pad);
		case METROID4:
			return MetroidFusionInput(pad);

		// TMNT
		case TMNT1:
			return TMNT1Input(pad);
		case TMNT2:
			return TMNT2Input(pad);
		case TMNT3:
			return TMNT3Input(pad);
		case TMNTGBA:
			return TMNTGBAInput(pad);
		case TMNTGBA2:
			return TMNTGBA2Input(pad);
		case TMNT:
			return TMNTInput(pad);

		// Medal Of Honor
		case MOHUNDERGROUND:
			return MohUndergroundInput(pad);
		case MOHINFILTRATOR:
			return MohInfiltratorInput(pad);

		// Harry Potter
		case HARRYPOTTER1GBC:
			return HarryPotter1GBCInput(pad);
		case HARRYPOTTER2GBC:
			return HarryPotter2GBCInput(pad);
		case HARRYPOTTER1:
			return HarryPotter1Input(pad);
		case HARRYPOTTER2:
			return HarryPotter2Input(pad);
		case HARRYPOTTER3:
			return HarryPotter3Input(pad);
		case HARRYPOTTER4:
			return HarryPotter4Input(pad);
		case HARRYPOTTER5:
			return HarryPotter5Input(pad);

		// Mario
		case MARIO1CLASSIC:
		case MARIO2CLASSIC:
			return Mario1ClassicInput(pad);
		case MARIO1DX:
			return Mario1DXInput(pad);
		case MARIO2ADV:
			return Mario2Input(pad);
		case MARIO3ADV:
			return Mario3Input(pad);
		case MARIOWORLD:
			return MarioWorldInput(pad);
		case YOSHIISLAND:
			return YoshiIslandInput(pad);
		case MARIOLAND1:
			return MarioLand1Input(pad);
		case MARIOLAND2:
			return MarioLand2Input(pad);
		case YOSHIUG:
			return UniversalGravitationInput(pad);

		// Mario Kart
		case MARIOKART:
			return MarioKartInput(pad);

		// Lego Star Wars
		case LSW1:
			return LegoStarWars1Input(pad);
		case LSW2:
			return LegoStarWars2Input(pad);

		// Star Wars
		case SWOBIWAN:
			return SWObiWanInput(pad);
		case SWEP2:
			return SWEpisode2Input(pad);

		// Mortal Kombat
		case MK1:
			return MK1Input(pad);
		case MK12:
			return MK12Input(pad);
		case MK2:
			return MK2Input(pad);
		case MK3:
			return MK3Input(pad);
		case MK4:
			return MK4Input(pad);
		case MKA:
			return MKAInput(pad);
		case MKDA:
			return MKDAInput(pad);
		case MKTE:
			return MKTEInput(pad);

		// WarioWare
		case TWISTED:
			return TwistedInput(pad);

		// Kirby
		case KIRBYTNT:
		case KIRBYTNTJ:
			return KirbyTntInput(pad);

		// Boktai
		case BOKTAI1:
			return BoktaiInput(pad);
		case BOKTAI2:
		case BOKTAI3:
			return Boktai2Input(pad);

		// One Piece
		case ONEPIECE:
			return OnePieceInput(pad);
		
		// Lord of the Rings
		case HOBBIT:
			return HobbitInput(pad);
		case LOTR1:
			return FellowshipOfTheRingInput(pad);
		case LOTR2:
		case LOTR3:
			return ReturnOfTheKingInput(pad);
		
		// Castlevania
		case CVADVENTURE:
			return CastlevaniaAdventureInput(pad);
		case CVBELMONT:
			return CastlevaniaBelmontInput(pad);
		case CVLEGENDS:
			return CastlevaniaLegendsInput(pad);
		case CVCIRCLEMOON:
		case CVHARMONY:
		case CVARIA:
			return CastlevaniaCircleMoonInput(pad);
	}

	// the function result, J, is a combination of flags for all the VBA buttons that are down
	u32 J = StandardMovement(pad);

	// Turbo feature
	if(
		userInput[0].pad.substickX > 70 ||
		userInput[0].WPAD_Stick(1,0) > 70
	)
		J |= VBA_SPEED;

	// Report pressed buttons (gamepads)
	int i;

	for (i = 0; i < MAXJP; i++)
	{
		if ((userInput[pad].pad.btns_h & btnmap[CTRLR_GCPAD][i]) // gamecube controller
		#ifdef HW_RVL
		|| ( (userInput[pad].wpad.exp.type == WPAD_EXP_NONE) && (userInput[pad].wpad.btns_h & btnmap[CTRLR_WIIMOTE][i]) ) // wiimote
		|| ( (userInput[pad].wpad.exp.type == WPAD_EXP_CLASSIC) && (userInput[pad].wpad.btns_h & btnmap[CTRLR_CLASSIC][i]) ) // classic controller
		|| ( (userInput[pad].wpad.exp.type == WPAD_EXP_NUNCHUK) && (userInput[pad].wpad.btns_h & btnmap[CTRLR_NUNCHUK][i]) ) // nunchuk + wiimote
		|| ( (DownUsbKeys[btnmap[CTRLR_KEYBOARD][i]]) ) // keyboard
		#endif
		)
			J |= vbapadmap[i];
	}

	return J;
}

u32 GetJoy(int pad)
{
	// request to go back to menu
	if (
		(userInput[pad].pad.substickX < -70) ||
		(userInput[pad].wpad.btns_h & WPAD_BUTTON_HOME) ||
		(userInput[pad].wpad.btns_h & WPAD_CLASSIC_BUTTON_HOME) ||
		(DownUsbKeys[KB_ESC])
	)
	{
		ScreenshotRequested = 1;
		updateRumbleFrame();
		return 0;
	}

	u32 J = DecodeJoy(pad);
	// don't allow up+down or left+right
	if ((J & 48) == 48)
		J &= ~16;
	if ((J & 192) == 192)
		J &= ~128;
	updateRumbleFrame();
	return J;
}

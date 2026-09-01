/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * input.cpp
 *
 * Wii/Gamecube controller management
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>

#include "vbagx.h"
#include "button_mapping.h"
#include "audio.h"
#include "video.h"
#include "input.h"
#include "gameinput.h"
#include "libgui/Gui.h"
#include "drivers/ogc/wiidrc.h"
#include "vbasupport.h"
#include "vba/gba/GBA.h"
#include "vba/gba/bios.h"
#include "vba/gba/GBAinline.h"

#define ANALOG_SENSITIVITY 30

int playerMapping[4] = {0,1,2,3};

static bool cartridgeRumble = false, possibleCartridgeRumble = false;
static int gameRumbleCount = 0, menuRumbleCount = 0, rumbleCountAlready = 0;

static unsigned int vbapadmap[10]; // VBA controller buttons
uint32_t btnmap[6][10]; // button mapping

void ResetControls(int wiiCtrl)
{
	int i;

	// VBA controller buttons
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
	if(wiiCtrl == GUI_HW_GAMECUBE || wiiCtrl == -1)
	{
		i=0;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_B;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_A;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_TRIGGER_ZR; // GC Z button
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_PLUS;   // GC Start
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_UP;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_DOWN;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_LEFT;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_RIGHT;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_TRIGGER_L;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_TRIGGER_R;
	}

	/*** Wiimote Padmap (Sideways) ***/
	if(wiiCtrl == GUI_HW_WIIMOTE || wiiCtrl == -1)
	{
		i=0;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_1;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_2;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_MINUS;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_PLUS;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_RIGHT;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_LEFT;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_UP;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_DOWN;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_B;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_A;
	}

	/*** Classic Controller Padmap ***/
	if(wiiCtrl == GUI_HW_CLASSIC || wiiCtrl == -1)
	{
		i=0;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_Y;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_B;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_MINUS;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_PLUS;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_UP;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_DOWN;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_LEFT;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_RIGHT;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_TRIGGER_L;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_TRIGGER_R;
	}

	/*** Nunchuk + Wiimote Padmap ***/
	if(wiiCtrl == GUI_HW_NUNCHUK || wiiCtrl == -1)
	{
		i=0;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_TRIGGER_L;  // C mapped to L
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_TRIGGER_ZL; // Z mapped to ZL
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_MINUS;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_PLUS;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_UP;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_DOWN;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_LEFT;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_RIGHT;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_2;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_1;
	}
	
	/*** Wii U Pro Controller Padmap ***/
	if(wiiCtrl == GUI_HW_WUPC || wiiCtrl == -1)
	{
		i=0;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_Y;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_B;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_MINUS;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_PLUS;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_UP;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_DOWN;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_LEFT;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_RIGHT;
		btnmap[GUI_HW_WUPC][i++] = GUI_TRIGGER_L;
		btnmap[GUI_HW_WUPC][i++] = GUI_TRIGGER_R;
	}
	
	/*** Wii U Gamepad (DRC) Padmap ***/
	if(wiiCtrl == GUI_HW_DRC || wiiCtrl == -1)
	{
		i=0;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_Y;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_B;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_MINUS;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_PLUS;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_UP;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_DOWN;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_LEFT;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_RIGHT;
		btnmap[GUI_HW_DRC][i++] = GUI_TRIGGER_L;
		btnmap[GUI_HW_DRC][i++] = GUI_TRIGGER_R;
	}
}

static inline float clampf(float v, float lo, float hi)
{
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

/****************************************************************************
 * Hardware Mapping Helpers
 * Translates raw libogc hardware bits to our generic UI masks
 ***************************************************************************/
static uint32_t MapPADToGeneric(uint32_t pad_btns)
{
	uint32_t mask = GUI_BTN_NONE;
	if (pad_btns & PAD_BUTTON_A)      mask |= GUI_BTN_A;
	if (pad_btns & PAD_BUTTON_B)      mask |= GUI_BTN_B;
	if (pad_btns & PAD_BUTTON_X)      mask |= GUI_BTN_X;
	if (pad_btns & PAD_BUTTON_Y)      mask |= GUI_BTN_Y;
	if (pad_btns & PAD_BUTTON_UP)     mask |= GUI_BTN_UP;
	if (pad_btns & PAD_BUTTON_DOWN)   mask |= GUI_BTN_DOWN;
	if (pad_btns & PAD_BUTTON_LEFT)   mask |= GUI_BTN_LEFT;
	if (pad_btns & PAD_BUTTON_RIGHT)  mask |= GUI_BTN_RIGHT;
	if (pad_btns & PAD_BUTTON_START)  mask |= GUI_BTN_PLUS;
	if (pad_btns & PAD_TRIGGER_L)     mask |= GUI_TRIGGER_L;
	if (pad_btns & PAD_TRIGGER_R)     mask |= GUI_TRIGGER_R;
	if (pad_btns & PAD_TRIGGER_Z)     mask |= GUI_TRIGGER_ZR;
	return mask;
}

#ifdef HW_RVL
static uint32_t MapWiimoteToGeneric(uint32_t wpad_btns)
{
	uint32_t mask = GUI_BTN_NONE;

	if (wpad_btns & WPAD_BUTTON_A)     mask |= GUI_BTN_A;
	if (wpad_btns & WPAD_BUTTON_B)     mask |= GUI_BTN_B;
	if (wpad_btns & WPAD_BUTTON_1)     mask |= GUI_BTN_1;
	if (wpad_btns & WPAD_BUTTON_2)     mask |= GUI_BTN_2;
	if (wpad_btns & WPAD_BUTTON_UP)    mask |= GUI_BTN_UP;
	if (wpad_btns & WPAD_BUTTON_DOWN)  mask |= GUI_BTN_DOWN;
	if (wpad_btns & WPAD_BUTTON_LEFT)  mask |= GUI_BTN_LEFT;
	if (wpad_btns & WPAD_BUTTON_RIGHT) mask |= GUI_BTN_RIGHT;
	if (wpad_btns & WPAD_BUTTON_PLUS)  mask |= GUI_BTN_PLUS;
	if (wpad_btns & WPAD_BUTTON_MINUS) mask |= GUI_BTN_MINUS;
	if (wpad_btns & WPAD_BUTTON_HOME)  mask |= GUI_BTN_HOME;

	return mask;
}

static uint32_t MapNunchukToGeneric(uint32_t wpad_btns)
{
	uint32_t mask = GUI_BTN_NONE;

	if (wpad_btns & WPAD_NUNCHUK_BUTTON_Z) mask |= GUI_TRIGGER_ZL;
	if (wpad_btns & WPAD_NUNCHUK_BUTTON_C) mask |= GUI_TRIGGER_L;

	return mask;
}

static uint32_t MapClassicToGeneric(uint32_t wpad_btns)
{
	uint32_t mask = GUI_BTN_NONE;

	// Classic Controller inputs (upper 16 bits)
	if (wpad_btns & WPAD_CLASSIC_BUTTON_A) mask |= GUI_BTN_A;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_B) mask |= GUI_BTN_B;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_X) mask |= GUI_BTN_X;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_Y) mask |= GUI_BTN_Y;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_UP) mask |= GUI_BTN_UP;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_DOWN) mask |= GUI_BTN_DOWN;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_LEFT) mask |= GUI_BTN_LEFT;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_RIGHT) mask |= GUI_BTN_RIGHT;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_PLUS) mask |= GUI_BTN_PLUS;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_MINUS) mask |= GUI_BTN_MINUS;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_HOME) mask |= GUI_BTN_HOME;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_FULL_L) mask |= GUI_TRIGGER_L;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_FULL_R) mask |= GUI_TRIGGER_R;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_ZL) mask |= GUI_TRIGGER_ZL;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_ZR) mask |= GUI_TRIGGER_ZR;

	return mask;
}

static uint32_t MapWiiUGamepadToGeneric(uint32_t drc_btns)
{
	uint32_t mask = GUI_BTN_NONE;
	if (drc_btns & WIIDRC_BUTTON_A) mask |= GUI_BTN_A;
	if (drc_btns & WIIDRC_BUTTON_B) mask |= GUI_BTN_B;
	if (drc_btns & WIIDRC_BUTTON_X) mask |= GUI_BTN_X;
	if (drc_btns & WIIDRC_BUTTON_Y) mask |= GUI_BTN_Y;
	if (drc_btns & WIIDRC_BUTTON_UP) mask |= GUI_BTN_UP;
	if (drc_btns & WIIDRC_BUTTON_DOWN) mask |= GUI_BTN_DOWN;
	if (drc_btns & WIIDRC_BUTTON_LEFT) mask |= GUI_BTN_LEFT;
	if (drc_btns & WIIDRC_BUTTON_RIGHT) mask |= GUI_BTN_RIGHT;
	if (drc_btns & WIIDRC_BUTTON_PLUS) mask |= GUI_BTN_PLUS;
	if (drc_btns & WIIDRC_BUTTON_MINUS) mask |= GUI_BTN_MINUS;
	if (drc_btns & WIIDRC_BUTTON_HOME) mask |= GUI_BTN_HOME;
	if (drc_btns & WIIDRC_BUTTON_L) mask |= GUI_TRIGGER_L;
	if (drc_btns & WIIDRC_BUTTON_R) mask |= GUI_TRIGGER_R;
	if (drc_btns & WIIDRC_BUTTON_ZL) mask |= GUI_TRIGGER_ZL;
	if (drc_btns & WIIDRC_BUTTON_ZR) mask |= GUI_TRIGGER_ZR;
	return mask;
}

/****************************************************************************
 * Analog Normalization Helpers
 ***************************************************************************/
static float NormalizeWPADAnalog(int pos, int min, int max, int center)
{
	if (min == max) return 0.0f;

	// Handle broken 3rd party controller calibration data
	if ((min >= center) || (max <= center)) {
		min = 0; max = 64; center = 32; // Generic fallback
	}

	int offset = pos - center;
	if (offset > 0) {
		return clampf((float)offset / (float)(max - center), 0.0f, 1.0f);
	} else {
		return clampf((float)offset / (float)(center - min), -1.0f, 0.0f);
	}
}
#endif

/****************************************************************************
 * UpdatePads
 * Scans all controllers, combines states, and updates controllers
 ***************************************************************************/
void UpdatePads()
{
	#ifdef HW_RVL
	WiiDRC_ScanPads();
	WPAD_ScanPads();
	#endif

	uint32_t activeGamecubePads = PAD_ScanPads();

	float deltaTime = 1.0f / 60.0f;

	for(int i = 3; i >= 0; i--) {
		GuiInputPadData padData;

		bool gamecubeActive = (activeGamecubePads & (1 << i)) != 0;

		if(gamecubeActive) {
			padData.hw_connected[GUI_HW_GAMECUBE] = true;
			padData.hw_buttons_d[GUI_HW_GAMECUBE] = MapPADToGeneric(PAD_ButtonsDown(i));
			padData.hw_buttons_h[GUI_HW_GAMECUBE] = MapPADToGeneric(PAD_ButtonsHeld(i));
			padData.hw_buttons_r[GUI_HW_GAMECUBE] = MapPADToGeneric(PAD_ButtonsUp(i));
			padData.hw_stickX[GUI_HW_GAMECUBE] = clampf((float)PAD_StickX(i) / 128.0f, -1.0f, 1.0f);
			padData.hw_stickY[GUI_HW_GAMECUBE] = clampf((float)PAD_StickY(i) / 128.0f, -1.0f, 1.0f);
			padData.hw_substickX[GUI_HW_GAMECUBE] = clampf((float)PAD_SubStickX(i) / 128.0f, -1.0f, 1.0f);
			padData.hw_substickY[GUI_HW_GAMECUBE] = clampf((float)PAD_SubStickY(i) / 128.0f, -1.0f, 1.0f);
		}

		#ifdef HW_RVL
		// Process Wiimote and Extensions
		uint32_t exp_type = WPAD_EXP_NONE;

		if (WPAD_Probe(i, &exp_type) == WPAD_ERR_NONE) {
			WPADData* wpad = WPAD_Data(i);

			// Always process base Wiimote
			padData.hw_connected[GUI_HW_WIIMOTE] = true;
			padData.battery_level = wpad->battery_level;
			
			padData.hw_gforceX[GUI_HW_WIIMOTE] = wpad->gforce.x;
			padData.hw_gforceY[GUI_HW_WIIMOTE] = wpad->gforce.y;
			padData.hw_gforceZ[GUI_HW_WIIMOTE] = wpad->gforce.z;
			padData.hw_pitch[GUI_HW_WIIMOTE]   = wpad->orient.pitch;
			padData.hw_roll[GUI_HW_WIIMOTE]    = wpad->orient.roll;
			padData.hw_yaw[GUI_HW_WIIMOTE]     = wpad->orient.yaw;

			if (exp_type == WPAD_EXP_NONE) {
				padData.hw_buttons_d[GUI_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_d);
				padData.hw_buttons_h[GUI_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_h);
				padData.hw_buttons_r[GUI_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_u);

				if (wpad->ir.valid) {
					padData.validPointer = true;
					padData.isTouch = false;
					padData.cursor_x = wpad->ir.x;
					padData.cursor_y = wpad->ir.y;
					padData.cursor_angle = wpad->ir.angle;
				}

				userInput[i]->setSideways(fabs(wpad->gforce.x) > fabs(wpad->gforce.y));
			}
			else if (exp_type == WPAD_EXP_NUNCHUK) {
				padData.hw_buttons_d[GUI_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_d);
				padData.hw_buttons_h[GUI_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_h);
				padData.hw_buttons_r[GUI_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_u);

				padData.hw_connected[GUI_HW_NUNCHUK] = true;
				padData.hw_buttons_d[GUI_HW_NUNCHUK] = MapNunchukToGeneric(wpad->btns_d);
				padData.hw_buttons_h[GUI_HW_NUNCHUK] = MapNunchukToGeneric(wpad->btns_h);
				padData.hw_buttons_r[GUI_HW_NUNCHUK] = MapNunchukToGeneric(wpad->btns_u);
				joystick_t* js = &wpad->exp.nunchuk.js;
				padData.hw_stickX[GUI_HW_NUNCHUK] = NormalizeWPADAnalog(js->pos.x, js->min.x, js->max.x, js->center.x);
				padData.hw_stickY[GUI_HW_NUNCHUK] = NormalizeWPADAnalog(js->pos.y, js->min.y, js->max.y, js->center.y);
				
				padData.hw_gforceX[GUI_HW_NUNCHUK] = wpad->exp.nunchuk.gforce.x;
				padData.hw_gforceY[GUI_HW_NUNCHUK] = wpad->exp.nunchuk.gforce.y;
				padData.hw_gforceZ[GUI_HW_NUNCHUK] = wpad->exp.nunchuk.gforce.z;
				padData.hw_pitch[GUI_HW_NUNCHUK]   = wpad->exp.nunchuk.orient.pitch;
				padData.hw_roll[GUI_HW_NUNCHUK]    = wpad->exp.nunchuk.orient.roll;
				padData.hw_yaw[GUI_HW_NUNCHUK]     = wpad->exp.nunchuk.orient.yaw;
				
				userInput[i]->setSideways(false);
			}
			else if (exp_type == WPAD_EXP_CLASSIC) {
				bool isWUPC = (wpad->exp.classic.type == 2);
				int hw = isWUPC ? GUI_HW_WUPC : GUI_HW_CLASSIC;

				padData.hw_connected[hw] = true;
				padData.hw_buttons_d[hw] = MapClassicToGeneric(wpad->btns_d);
				padData.hw_buttons_h[hw] = MapClassicToGeneric(wpad->btns_h);
				padData.hw_buttons_r[hw] = MapClassicToGeneric(wpad->btns_u);

				joystick_t* ljs = &wpad->exp.classic.ljs;
				joystick_t* rjs = &wpad->exp.classic.rjs;
				padData.hw_stickX[hw] = NormalizeWPADAnalog(ljs->pos.x, ljs->min.x, ljs->max.x, ljs->center.x);
				padData.hw_stickY[hw] = NormalizeWPADAnalog(ljs->pos.y, ljs->min.y, ljs->max.y, ljs->center.y);
				padData.hw_substickX[hw] = NormalizeWPADAnalog(rjs->pos.x, rjs->min.x, rjs->max.x, rjs->center.x);
				padData.hw_substickY[hw] = NormalizeWPADAnalog(rjs->pos.y, rjs->min.y, rjs->max.y, rjs->center.y);
				userInput[i]->setSideways(false);
			}
			else {
				userInput[i]->setSideways(false);
			}
		}

		// Process Wii U Gamepad
		if(i == 0 && WiiDRC_Inited() && WiiDRC_Connected()) {
			padData.hw_connected[GUI_HW_DRC] = true;
			padData.hw_buttons_d[GUI_HW_DRC] = MapWiiUGamepadToGeneric(WiiDRC_ButtonsDown());
			padData.hw_buttons_h[GUI_HW_DRC] = MapWiiUGamepadToGeneric(WiiDRC_ButtonsHeld());
			padData.hw_buttons_r[GUI_HW_DRC] = MapWiiUGamepadToGeneric(WiiDRC_ButtonsUp());
			padData.hw_stickX[GUI_HW_DRC] = clampf((float)WiiDRC_lStickX() / 128.0f, -1.0f, 1.0f);
			padData.hw_stickY[GUI_HW_DRC] = clampf((float)WiiDRC_lStickY() / 128.0f, -1.0f, 1.0f);
			padData.hw_substickX[GUI_HW_DRC] = clampf((float)WiiDRC_rStickX() / 128.0f, -1.0f, 1.0f);
			padData.hw_substickY[GUI_HW_DRC] = clampf((float)WiiDRC_rStickY() / 128.0f, -1.0f, 1.0f);
		}
		#endif

		// 4. Merge into unified aggregate state for UI Elements
		for (uint32_t hw = 0; hw < GUI_HW_MAX; hw++) {
			if (!padData.hw_connected[hw])
				continue;

			padData.buttons_d |= padData.hw_buttons_d[hw];
			padData.buttons_h |= padData.hw_buttons_h[hw];
			padData.buttons_r |= padData.hw_buttons_r[hw];

			if (std::abs(padData.hw_stickX[hw]) > std::abs(padData.stickX)) padData.stickX = padData.hw_stickX[hw];
			if (std::abs(padData.hw_stickY[hw]) > std::abs(padData.stickY)) padData.stickY = padData.hw_stickY[hw];
			if (std::abs(padData.hw_substickX[hw]) > std::abs(padData.substickX)) padData.substickX = padData.hw_substickX[hw];
			if (std::abs(padData.hw_substickY[hw]) > std::abs(padData.substickY)) padData.substickY = padData.hw_substickY[hw];
		}

		// Push the finalized, merged payload to the controller abstraction
		userInput[i]->update(padData, deltaTime);
	}
}

/****************************************************************************
 * SetupPads
 * Allocates controllers and initializes hardware
 ***************************************************************************/
void SetupPads()
{
	PAD_Init();

	#ifdef HW_RVL
	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL, platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	#endif

	for(int i = 0; i < 4; i++) {
		userInput[i] = new GuiInputController(i);
	}
}

static int SilenceNeeded = 0;

static void updateRumble()
{
	if(!GCSettings.Rumble) return;

	bool r = false;
	if (MenuRequested) r = (menuRumbleCount > 0);
	else r = cartridgeRumble || possibleCartridgeRumble || (gameRumbleCount > 0) || (menuRumbleCount > 0);

	if (SilenceNeeded > 0)
	{
		if (r)
		{
			SilenceNeeded = 5;
			// It will always be greater than 0 after that!
			r = false;
		}
		else
		{
			if (--SilenceNeeded > 0) r = false;
		}
	}

#ifdef HW_RVL
	// Rumble wii remote 0
	WPAD_Rumble(0, r);
#endif
	PAD_ControlMotor(PAD_CHAN0, r?PAD_MOTOR_RUMBLE:PAD_MOTOR_STOP);
}

void updateRumbleFrame()
{
	if(!GCSettings.Rumble) return;

	// If we already rumbled continuously for more than 50 frames,
	// then disable rumbling for a while.
	if (rumbleCountAlready > 70) {
		SilenceNeeded = 5;
		rumbleCountAlready = 0;
	} else if (MenuRequested) {
		if (menuRumbleCount>0) ++rumbleCountAlready;
		else rumbleCountAlready=0;
	} else {
		if (gameRumbleCount>0 || menuRumbleCount>0 || possibleCartridgeRumble)
			++rumbleCountAlready;
		else rumbleCountAlready=0;
	}
	updateRumble();
	if (gameRumbleCount>0 && !MenuRequested) --gameRumbleCount;
	if (menuRumbleCount>0) --menuRumbleCount;
}

void systemPossibleCartridgeRumble(bool RumbleOn) {
	possibleCartridgeRumble = RumbleOn;
	updateRumble();
}

void systemCartridgeRumble(bool RumbleOn) {
	cartridgeRumble = RumbleOn;
	possibleCartridgeRumble = false;
	updateRumble();
}

void systemGameRumble(int RumbleForFrames) {
	if (RumbleForFrames > gameRumbleCount) gameRumbleCount = RumbleForFrames;
}

void systemGameRumbleOnlyFor(int OnlyRumbleForFrames) {
	gameRumbleCount = OnlyRumbleForFrames;
}

uint32_t StandardMovement(unsigned short chan)
{
	if (!userInput[chan]) return 0;
	const GuiInputPadData& pad = userInput[chan]->getPadData();
	uint32_t J = 0;
	
	float sensitivity = (float)ANALOG_SENSITIVITY / 128.0f;
	if (pad.stickY > sensitivity) J |= VBA_UP;
	else if (pad.stickY < -sensitivity) J |= VBA_DOWN;
	if (pad.stickX < -sensitivity) J |= VBA_LEFT;
	else if (pad.stickX > sensitivity) J |= VBA_RIGHT;

	return J;
}

uint32_t StandardDPad(unsigned short pad)
{
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = 0;
	if (data.buttons_h & GUI_BTN_UP) J |= VBA_UP;
	if (data.buttons_h & GUI_BTN_DOWN) J |= VBA_DOWN;
	if (data.buttons_h & GUI_BTN_LEFT) J |= VBA_LEFT;
	if (data.buttons_h & GUI_BTN_RIGHT) J |= VBA_RIGHT;
	return J;
}

uint32_t StandardSideways(unsigned short pad)
{
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = 0;

	// Read directly from isolated Wiimote state
	uint32_t wp = data.hw_buttons_h[GUI_HW_WIIMOTE];

	if (wp & GUI_BTN_RIGHT) J |= VBA_UP;
	else if (wp & GUI_BTN_LEFT) J |= VBA_DOWN;
	if (wp & GUI_BTN_UP) J |= VBA_LEFT;
	else if (wp & GUI_BTN_DOWN) J |= VBA_RIGHT;

	if (wp & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (wp & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	if (wp & GUI_BTN_1) J |= VBA_BUTTON_B;
	if (wp & GUI_BTN_2) J |= VBA_BUTTON_A;

	if (cartridgeType == CARTRIDGE_GBA) {
		if (wp & GUI_BTN_A) J |= VBA_BUTTON_R;
		if (wp & GUI_BTN_B) J |= VBA_BUTTON_L;
	} else {
		if ((wp & GUI_BTN_B) || (wp & GUI_BTN_A)) J |= VBA_SPEED;
	}
	return J;
}

uint32_t StandardClassic(unsigned short pad)
{
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = 0;

	// Read isolated Classic Controller state
	uint32_t wp = data.hw_buttons_h[GUI_HW_CLASSIC];

	if (wp & GUI_BTN_RIGHT) J |= VBA_RIGHT;
	else if (wp & GUI_BTN_LEFT) J |= VBA_LEFT;
	if (wp & GUI_BTN_UP) J |= VBA_UP;
	else if (wp & GUI_BTN_DOWN) J |= VBA_DOWN;

	if (wp & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (wp & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	if ((wp & GUI_TRIGGER_L) || (wp & GUI_TRIGGER_ZL)) J |= VBA_BUTTON_L;
	if ((wp & GUI_TRIGGER_R) || (wp & GUI_TRIGGER_ZR)) J |= VBA_BUTTON_R;

	if (wp & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (wp & GUI_BTN_B) J |= VBA_BUTTON_B;
	if ((wp & GUI_BTN_Y) || (wp & GUI_BTN_X)) J |= VBA_SPEED;

	return J;
}

/****************************************************************************
 * DecodeJoy
 *
 * Reads the STATE (not changes) from a controller and reports
 * this STATE (not changes) to VBA
 ****************************************************************************/
static uint32_t DecodeJoy(unsigned short pad)
{
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	CursorX = data.cursor_x;
	CursorY = data.cursor_y;
	CursorValid = data.validPointer;

	// check for games that should have special Wii controls
	if (GCSettings.WiiControls) {
		switch (RomIdCode & 0xFFFFFF) {
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
			case SWJPB:
				return SWJediPowerBattlesInput(pad);
			case SWEP2:
				return SWEpisode2Input(pad);
			case SWEP3:
				return SWEpisode3Input(pad);
			case SWEP4:
				return SWEpisode4Input(pad);
			case SWEP5:
				return SWEpisode5Input(pad);
			case SWEP6:
				return SWEpisode6Input(pad);
			case SWTRILOGY:
				return SWTrilogyInput(pad);
			case SWNDA:
				return SWNDAInput(pad);
			case SWYODA:
				return SWYodaStoriesInput(pad);

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

			case KIDDRACULA:
				return KidDraculaInput(pad);
		}
	}

	// Get baseline movement (translates unified Analog Sticks to VBA directions)
	uint32_t J = StandardMovement(pad);

	// Evaluate Turbo (C-Stick Right / Right Stick Right)
	if (GCSettings.TurboModeEnabled)
	{
		if (data.substickX > 0.55f) {
			J |= VBA_SPEED;
		}
	}

	// Evaluate Standard Buttons
	for (int i = 0; i < 10; ++i)
	{
		bool button_pressed = false;

		// Check if ANY connected hardware matches the mapping
		for (uint32_t hw = 0; hw < GUI_HW_MAX; hw++)
		{
			if (!data.hw_connected[hw]) continue;
			uint32_t mapped_btn = btnmap[hw][i];

			if (data.hw_buttons_h[hw] & mapped_btn) {
				button_pressed = true;
				break;
			}
		}

		if (button_pressed) {
			J |= vbapadmap[i];
		}
	}

	return J;
}

bool isMenuRequested()
{
	for(int i=0; i<4; i++) {
		if (!userInput[i]) continue;
		const GuiInputPadData& pad = userInput[i]->getPadData();

		bool rightStickLeft = (pad.substickX < -0.55f);
		bool homePressed = (pad.buttons_h & GUI_BTN_HOME);
		bool lPlusRPlusStart = (pad.buttons_h & GUI_TRIGGER_L) &&
							   (pad.buttons_h & GUI_TRIGGER_R) &&
							   (pad.buttons_h & GUI_BTN_PLUS);

		if (rightStickLeft || lPlusRPlusStart || homePressed)
		{
			return true; 
		}
	}
	return false;
}

static int GetPlayerChan(int pad)
{
	for(int i=3; i >= 0; i--) {
		if(playerMapping[i] == pad) {
			return i;
		}
	}
	return pad;
}

uint32_t GetJoy(int pad)
{
	// request to go back to menu
	if (isMenuRequested())
	{
		MenuRequested = true;
		updateRumbleFrame();
		return 0;
	}

	int chan = GetPlayerChan(pad);

	uint32_t J = DecodeJoy(chan);
	// don't allow up+down or left+right
	if ((J & 48) == 48)
		J &= ~16;
	if ((J & 192) == 192)
		J &= ~128;
	updateRumbleFrame();

	return J;
}

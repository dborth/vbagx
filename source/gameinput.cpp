/****************************************************************************
 * Visual Boy Advance GX
 *
 * Carl Kenner Febuary 2009
 * Daryl Borth 2026 (Decoupled Input Architecture)
 *
 * gameinput.cpp
 *
 * Wii/Gamecube/Wii U controls for individual games
 ***************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "vbagx.h"
#include "button_mapping.h"
#include "video.h"
#include "input.h"
#include "gameinput.h"
#include "vbasupport.h"
#include "libgui/GuiInputController.h"

#include "vba/gba/GBA.h"
#include "vba/gba/bios.h"
#include "vba/gba/GBAinline.h"

extern bool CalibrateWario;

char DebugStr[50] = "";

void DebugPrintf(const char *format, ...) {
	va_list args;
	va_start( args, format );
	vsprintf( DebugStr, format, args );
	va_end( args );
}

uint32_t TMNTInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardDPad(pad);
	static uint32_t LastDir = VBA_RIGHT;
	static bool wait = false;
	static int holdcount = 0;
	bool Jump=0, Attack=0, SpinKick=0, Roll=0, Pause=0, Select=0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		Jump = (hw & GUI_BTN_A);
		Attack = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5);
		SpinKick = (fabs(data.hw_gforceX[GUI_HW_NUNCHUK]) > 0.5);
		Roll = (hw & GUI_TRIGGER_ZL) || (hw & GUI_TRIGGER_L); // Z or C
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
		// Swap Turtles or super turtle summon
		if (hw & GUI_BTN_B) {
			if (data.hw_pitch[GUI_HW_NUNCHUK] < -35 && data.hw_pitch[GUI_HW_WIIMOTE] < -35)
				J |= VBA_BUTTON_L | VBA_BUTTON_R;
			else J |= VBA_BUTTON_R;
		}
	} else if (data.hw_connected[GUI_HW_CLASSIC]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_CLASSIC];
		Jump = (hw & GUI_BTN_B);
		Attack = (hw & GUI_BTN_A);
		SpinKick = (hw & GUI_BTN_X);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
		Roll = (hw & (GUI_TRIGGER_L | GUI_TRIGGER_R | GUI_TRIGGER_ZL | GUI_TRIGGER_ZR));

		if (hw & GUI_BTN_Y) {
			holdcount++;
			if (holdcount > 20) J |= VBA_BUTTON_L | VBA_BUTTON_R;
		}
		if (data.hw_buttons_r[GUI_HW_CLASSIC] & GUI_BTN_Y) {
			if (holdcount <= 20) J |= VBA_BUTTON_R;
			holdcount = 0;
		}
	} else if (data.hw_connected[GUI_HW_WIIMOTE]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_WIIMOTE];
		Jump = (hw & GUI_BTN_A);
		Attack = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
		if (hw & GUI_BTN_B) {
			if (data.hw_pitch[GUI_HW_WIIMOTE] < -40)
				J |= VBA_BUTTON_L | VBA_BUTTON_R;
			else J |= VBA_BUTTON_R;
		}
		SpinKick = (hw & GUI_BTN_1);
		Roll = (hw & GUI_BTN_2);
	}

	uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
	uint32_t gc_r = data.hw_buttons_r[GUI_HW_GAMECUBE];

	if (gc & GUI_BTN_UP) J |= VBA_UP;
	if (gc & GUI_BTN_DOWN) J |= VBA_DOWN;
	if (gc & GUI_BTN_LEFT) J |= VBA_LEFT;
	if (gc & GUI_BTN_RIGHT) J |= VBA_RIGHT;
	if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;

	if (gc & GUI_BTN_B) {
		holdcount++;
		if (holdcount > 20) J |= VBA_BUTTON_L | VBA_BUTTON_R;
	}
	if (gc_r & GUI_BTN_B) {
		if (holdcount <= 20) J |= VBA_BUTTON_R;
		holdcount = 0;
	}

	if (gc & GUI_BTN_X) Attack = true;
	if (gc & GUI_BTN_Y) SpinKick = true;
	if (gc & GUI_BTN_PLUS) Pause = true;
	if (gc & GUI_TRIGGER_ZR) Select = true; // Z Button
	if ((gc & GUI_TRIGGER_L) || (gc & GUI_TRIGGER_R)) Roll = true;

	if (Jump) J |= VBA_BUTTON_A;
	if (Attack) J |= VBA_BUTTON_B;
	if (SpinKick) J |= VBA_BUTTON_B | VBA_BUTTON_A;
	if (Pause) J |= VBA_BUTTON_START;
	if (Select) J |= VBA_BUTTON_SELECT;
	if (Roll) {
		if (!wait) {
			J |= LastDir; // Double tap D-Pad to roll
			wait = true;
		} else wait = false;
	}
	
	if (J & VBA_RIGHT) LastDir = VBA_RIGHT;
	else if (J & VBA_LEFT) LastDir = VBA_LEFT;
	return J;
}

uint32_t TMNT1Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardDPad(pad);
	static uint32_t LastDir = VBA_RIGHT;
	bool Jump=0, Attack=0, SpinKick=0, Roll=0, Pause=0, Select=0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		Jump = (hw & GUI_BTN_A);
		Attack = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5);
		SpinKick = (fabs(data.hw_gforceX[GUI_HW_NUNCHUK]) > 0.5);
		Roll = (hw & GUI_TRIGGER_ZL) || (hw & GUI_TRIGGER_L);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
	} else if (data.hw_connected[GUI_HW_CLASSIC]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_CLASSIC];
		Jump = (hw & GUI_BTN_B);
		Attack = (hw & GUI_BTN_A);
		SpinKick = (hw & GUI_BTN_X);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
		Roll = (hw & (GUI_TRIGGER_L | GUI_TRIGGER_R | GUI_TRIGGER_ZL | GUI_TRIGGER_ZR));
	} else if (data.hw_connected[GUI_HW_WIIMOTE]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_WIIMOTE];
		Jump = (hw & GUI_BTN_A);
		Attack = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
		SpinKick = (hw & GUI_BTN_1);
		Roll = (hw & GUI_BTN_2);
	}

	uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
	if (gc & GUI_BTN_UP) J |= VBA_UP;
	if (gc & GUI_BTN_DOWN) J |= VBA_DOWN;
	if (gc & GUI_BTN_LEFT) J |= VBA_LEFT;
	if (gc & GUI_BTN_RIGHT) J |= VBA_RIGHT;
	if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (gc & GUI_BTN_X) Attack = true;
	if (gc & GUI_BTN_Y) SpinKick = true;
	if (gc & GUI_BTN_PLUS) Pause = true;
	if (gc & GUI_TRIGGER_ZR) Select = true;
	if ((gc & GUI_TRIGGER_L) || (gc & GUI_TRIGGER_R)) Roll = true;

	if (Jump) J |= VBA_BUTTON_A;
	if (Attack || SpinKick) J |= VBA_BUTTON_B;
	if (Pause) J |= VBA_BUTTON_START;
	if (Select) J |= VBA_BUTTON_SELECT;
		
	if (J & VBA_RIGHT) LastDir = VBA_RIGHT;
	else if (J & VBA_LEFT) LastDir = VBA_LEFT;
	return J;
}

uint32_t TMNT2Input(unsigned short pad) {
	// Functionally matches TMNT1 specific layout, using unified architecture
	return TMNT1Input(pad);
}

uint32_t TMNT3Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardDPad(pad);
	static uint32_t LastDir = VBA_RIGHT;
	bool Jump=0, Attack=0, SpinKick=0, Roll=0, Pause=0, Select=0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		Jump = (hw & GUI_BTN_A);
		Attack = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5);
		SpinKick = (fabs(data.hw_gforceX[GUI_HW_NUNCHUK]) > 0.5);
		Roll = (hw & GUI_TRIGGER_ZL) || (hw & GUI_TRIGGER_L);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
		if (hw & GUI_BTN_B) J |= VBA_BUTTON_START;
	} else if (data.hw_connected[GUI_HW_CLASSIC]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_CLASSIC];
		Jump = (hw & GUI_BTN_B);
		Attack = (hw & GUI_BTN_A);
		SpinKick = (hw & GUI_BTN_X);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
		Roll = (hw & (GUI_TRIGGER_L | GUI_TRIGGER_R | GUI_TRIGGER_ZL | GUI_TRIGGER_ZR));
		if (hw & GUI_BTN_Y) J |= VBA_BUTTON_START;
	} else if (data.hw_connected[GUI_HW_WIIMOTE]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_WIIMOTE];
		Jump = (hw & GUI_BTN_A);
		Attack = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
		if (hw & GUI_BTN_B) J |= VBA_BUTTON_START;
		SpinKick = (hw & GUI_BTN_1);
		Roll = (hw & GUI_BTN_2);
	}

	uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
	if (gc & GUI_BTN_UP) J |= VBA_UP;
	if (gc & GUI_BTN_DOWN) J |= VBA_DOWN;
	if (gc & GUI_BTN_LEFT) J |= VBA_LEFT;
	if (gc & GUI_BTN_RIGHT) J |= VBA_RIGHT;
	if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (gc & GUI_BTN_B) J |= VBA_BUTTON_START;
	if (gc & GUI_BTN_X) Attack = true;
	if (gc & GUI_BTN_Y) SpinKick = true;
	if (gc & GUI_BTN_PLUS) Pause = true;
	if (gc & GUI_TRIGGER_ZR) Select = true;
	if ((gc & GUI_TRIGGER_L) || (gc & GUI_TRIGGER_R)) Roll = true;

	if (Jump || Roll) J |= VBA_BUTTON_A;
	if (Attack || SpinKick) J |= VBA_BUTTON_B;
	if (Pause) J |= VBA_BUTTON_SELECT;
	if (Select) J |= VBA_BUTTON_START;
		
	if (J & VBA_RIGHT) LastDir = VBA_RIGHT;
	else if (J & VBA_LEFT) LastDir = VBA_LEFT;
	return J;
}

uint32_t TMNTGBAInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardDPad(pad);
	static uint32_t LastDir = VBA_RIGHT;
	bool Jump=0, Attack=0, SpinKick=0, SpecialMove=0, Pause=0, Select=0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		Jump = (hw & GUI_BTN_A);
		Attack = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5) || (hw & GUI_BTN_B);
		SpinKick = (fabs(data.hw_gforceX[GUI_HW_NUNCHUK]) > 0.5);
		SpecialMove = (hw & GUI_TRIGGER_ZL);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
	} else if (data.hw_connected[GUI_HW_CLASSIC]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_CLASSIC];
		Jump = (hw & GUI_BTN_B);
		Attack = (hw & GUI_BTN_A);
		SpinKick = (hw & GUI_BTN_X);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
		SpecialMove = (hw & (GUI_TRIGGER_L | GUI_TRIGGER_R | GUI_TRIGGER_ZL | GUI_TRIGGER_ZR));
	} else if (data.hw_connected[GUI_HW_WIIMOTE]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_WIIMOTE];
		Jump = (hw & GUI_BTN_A);
		Attack = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5) || (hw & GUI_BTN_B);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
		SpinKick = (hw & GUI_BTN_1);
		SpecialMove = (hw & GUI_BTN_2);
	}

	uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
	if (gc & GUI_BTN_UP) J |= VBA_UP;
	if (gc & GUI_BTN_DOWN) J |= VBA_DOWN;
	if (gc & GUI_BTN_LEFT) J |= VBA_LEFT;
	if (gc & GUI_BTN_RIGHT) J |= VBA_RIGHT;
	if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (gc & GUI_BTN_X) Attack = true;
	if (gc & GUI_BTN_Y) SpinKick = true;
	if (gc & GUI_BTN_PLUS) Pause = true;
	if (gc & GUI_TRIGGER_ZR) Select = true;
	if ((gc & GUI_TRIGGER_L) || (gc & GUI_TRIGGER_R)) SpecialMove = true;

	if (Jump) J |= VBA_BUTTON_A;
	if (Attack) J |= VBA_BUTTON_B;
	if (SpinKick) J |= VBA_BUTTON_R;
	if (Pause) J |= VBA_BUTTON_START;
	if (Select) J |= VBA_BUTTON_SELECT;
	if (SpecialMove) J |= VBA_BUTTON_R | VBA_BUTTON_A;
		
	if (J & VBA_RIGHT) LastDir = VBA_RIGHT;
	else if (J & VBA_LEFT) LastDir = VBA_LEFT;
	return J;
}

uint32_t TMNTGBA2Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardDPad(pad);
	static uint32_t LastDir = VBA_RIGHT;
	bool Jump=0, Attack=0, SpinKick=0, SpecialMove=0, Pause=0, Select=0, Look=0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		Jump = (hw & GUI_BTN_A);
		Attack = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5) || (hw & GUI_BTN_B);
		SpinKick = (fabs(data.hw_gforceX[GUI_HW_NUNCHUK]) > 0.5);
		SpecialMove = (hw & GUI_TRIGGER_ZL);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
		Look = (hw & GUI_TRIGGER_L);
	} else if (data.hw_connected[GUI_HW_CLASSIC]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_CLASSIC];
		Jump = (hw & GUI_BTN_B);
		Attack = (hw & GUI_BTN_A);
		SpinKick = (hw & GUI_BTN_X);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
		SpecialMove = (hw & (GUI_TRIGGER_L | GUI_TRIGGER_R | GUI_TRIGGER_ZR));
		Look = (hw & GUI_TRIGGER_ZL);
	} else if (data.hw_connected[GUI_HW_WIIMOTE]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_WIIMOTE];
		Jump = (hw & GUI_BTN_A);
		Attack = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5) || (hw & GUI_BTN_B);
		Pause = (hw & GUI_BTN_PLUS);
		Select = (hw & GUI_BTN_MINUS);
		if (hw & GUI_BTN_B) Look = true;
		SpinKick = (hw & GUI_BTN_1);
		SpecialMove = (hw & GUI_BTN_2);
	}

	uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
	if (gc & GUI_BTN_UP) J |= VBA_UP;
	if (gc & GUI_BTN_DOWN) J |= VBA_DOWN;
	if (gc & GUI_BTN_LEFT) J |= VBA_LEFT;
	if (gc & GUI_BTN_RIGHT) J |= VBA_RIGHT;
	if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (gc & GUI_BTN_B) Look = true;
	if (gc & GUI_BTN_X) Attack = true;
	if (gc & GUI_BTN_Y) SpinKick = true;
	if (gc & GUI_BTN_PLUS) Pause = true;
	if (gc & GUI_TRIGGER_ZR) Select = true;
	if ((gc & GUI_TRIGGER_L) || (gc & GUI_TRIGGER_R)) SpecialMove = true;

	if (Jump) J |= VBA_BUTTON_A;
	if (Attack) J |= VBA_BUTTON_B;
	if (SpinKick) J |= VBA_BUTTON_R;
	if (Pause) J |= VBA_BUTTON_START;
	if (Select) J |= VBA_BUTTON_SELECT;
	if (SpecialMove) J |= VBA_UP | VBA_BUTTON_A;
		
	if (J & VBA_RIGHT) LastDir = VBA_RIGHT;
	else if (J & VBA_LEFT) LastDir = VBA_LEFT;
	return J;
}

uint32_t HarryPotter1GBCInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardDPad(pad);

	// Apply standard mapping for GameCube specifically as requested by legacy code
	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	// Pause & Select
	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	// Core actions
	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (data.buttons_h & GUI_BTN_B) J |= VBA_BUTTON_B;

	// Spells via gforce missing in generic unified state, access directly via wiimote state
	if (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5) J |= VBA_BUTTON_A;

	if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1) J |= VBA_BUTTON_L | VBA_BUTTON_R;

	// Run
	if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_ZL) J |= VBA_SPEED;
	// Camera
	if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_L) J |= VBA_BUTTON_SELECT;

	return J;
}

uint32_t HarryPotter2GBCInput(unsigned short pad) {
	return HarryPotter1GBCInput(pad);
}

uint32_t HarryPotter1Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad);

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	// Generic fallback logic across controllers
	if (data.buttons_h & GUI_BTN_RIGHT) J |= VBA_BUTTON_R;
	if (data.buttons_h & GUI_BTN_LEFT) J |= VBA_BUTTON_L;
	if (data.buttons_h & GUI_BTN_UP) J |= VBA_UP;
	if (data.buttons_h & GUI_BTN_DOWN) J |= VBA_DOWN;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (data.buttons_h & GUI_BTN_B) J |= VBA_BUTTON_B;

	// Spells via gforce
	if (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5) J |= VBA_BUTTON_B;

	if (data.hw_buttons_h[GUI_HW_WIIMOTE] & (GUI_BTN_1 | GUI_BTN_2)) J |= VBA_BUTTON_R;

	// Run / Flute
	if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_ZL) J |= VBA_SPEED;
	if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_L) J |= VBA_BUTTON_L;

	return J;
}

uint32_t HarryPotter2Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	if (data.buttons_h & GUI_BTN_RIGHT) J |= VBA_BUTTON_R;
	if (data.buttons_h & GUI_BTN_LEFT) J |= VBA_BUTTON_L;
	if (data.buttons_h & GUI_BTN_UP) J |= VBA_UP;
	if (data.buttons_h & GUI_BTN_DOWN) J |= VBA_DOWN;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_B;
	if (data.buttons_h & GUI_BTN_B) J |= VBA_BUTTON_A;

	// Spells via gforce
	if (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5) J |= VBA_BUTTON_A;

	if (data.hw_buttons_h[GUI_HW_WIIMOTE] & (GUI_BTN_1 | GUI_BTN_2)) J |= VBA_BUTTON_L;

	// Sneak / Jump
	if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_ZL) J |= VBA_BUTTON_B;
	if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_L) J |= VBA_BUTTON_R;

	return J;
}

uint32_t HarryPotter3Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	if (data.buttons_h & GUI_BTN_RIGHT) J |= VBA_BUTTON_R;
	if (data.buttons_h & GUI_BTN_LEFT) J |= VBA_BUTTON_L;
	if (data.buttons_h & GUI_BTN_UP) J |= VBA_UP;
	if (data.buttons_h & GUI_BTN_DOWN) J |= VBA_DOWN;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (data.buttons_h & GUI_BTN_B) J |= VBA_BUTTON_B;

	// Spells via gforce
	if (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5) J |= VBA_BUTTON_B;

	if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1) J |= VBA_BUTTON_L;
	if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_2) J |= VBA_BUTTON_R;

	if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_ZL) J |= VBA_SPEED;

	return J;
}

uint32_t HarryPotter4Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	if (data.buttons_h & GUI_BTN_RIGHT) J |= VBA_BUTTON_R;
	if (data.buttons_h & GUI_BTN_LEFT) J |= VBA_BUTTON_L;
	if (data.buttons_h & GUI_BTN_UP) J |= VBA_UP;
	if (data.buttons_h & GUI_BTN_DOWN) J |= VBA_DOWN;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;

	// Spells via gforce
	if (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5) J |= VBA_BUTTON_A;

	if (data.buttons_h & GUI_BTN_B) J |= VBA_BUTTON_B;

	if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1) J |= VBA_BUTTON_L;
	if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_2) J |= VBA_BUTTON_R;

	if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_ZL) J |= VBA_SPEED;

	return J;
}

uint32_t HarryPotter5Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	// Wand cursor via unified IR/Touch valid pointer
	int cx = 0;
	int cy = 0;
	static int oldcx = 0;
	static int oldcy = 0;
	uint8_t WandOut = CPUReadByte(0x200e0dd);
	if (WandOut && data.validPointer) {
		cx = (data.cursor_x * 268) / 640;
		cy = (data.cursor_y * 187) / 480;
		if (cx<0x14) cx=0x14;
		else if (cx>0xf8) cx=0xf8;
		if (cy<0x13) cy=0x13;
		else if (cy>0xa8) cy=0xa8;
		CPUWriteByte(0x200e0fe, cx);
		CPUWriteByte(0x200e102, cy);
	}
	oldcx = cx;
	oldcy = cy;

	if (data.buttons_h & GUI_BTN_RIGHT) J |= VBA_BUTTON_R;
	if (data.buttons_h & GUI_BTN_LEFT) J |= VBA_BUTTON_L;
	if (data.buttons_h & GUI_BTN_UP) J |= VBA_UP;
	if (data.buttons_h & GUI_BTN_DOWN) J |= VBA_DOWN;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (data.buttons_h & GUI_BTN_B) J |= VBA_BUTTON_B;

	if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_ZL) J |= VBA_SPEED;

	return J;
}

// WarioWare Twisted
uint32_t TwistedInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		TiltSideways = false;
		J |= StandardDPad(pad);

		if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

		if (data.hw_buttons_h[GUI_HW_WIIMOTE] & (GUI_BTN_1 | GUI_BTN_2)) J |= VBA_BUTTON_L | VBA_SPEED;

		if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (data.buttons_h & GUI_BTN_B) J |= VBA_BUTTON_B;

		if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_ZL) J |= VBA_BUTTON_R;

		if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_L) {
			CalibrateWario = true;
		} else CalibrateWario = false;
	} else if (data.hw_connected[GUI_HW_CLASSIC]) {
		TiltSideways = false;
		J |= StandardDPad(pad) | StandardClassic(pad);
	} else {
		TiltSideways = true;
		J |= StandardSideways(pad);
		if (data.buttons_h & GUI_BTN_B) {
			CalibrateWario = true;
		} else CalibrateWario = false;
	}

	return J;
}

uint32_t KirbyTntInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		TiltSideways = false;
		J |= StandardDPad(pad);
		if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;
		if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1) J |= VBA_BUTTON_B;
		if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_2) J |= VBA_BUTTON_A;

		if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (data.buttons_h & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_L) J |= VBA_SPEED;
	} else if (data.hw_connected[GUI_HW_CLASSIC]) {
		TiltSideways = false;
		J |= StandardDPad(pad) | StandardClassic(pad);
		if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	} else {
		TiltSideways = true;
		J |= StandardSideways(pad);
		if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (data.buttons_h & GUI_BTN_B) J |= VBA_SPEED;
	}
	return J;
}

uint32_t MohInfiltratorInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		if (hw & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (hw & GUI_BTN_MINUS) J |= VBA_BUTTON_L;
		if (hw & GUI_BTN_A) J |= VBA_BUTTON_A | VBA_BUTTON_L;
		if (hw & GUI_BTN_B) J |= VBA_BUTTON_A;

		if (fabs(data.hw_gforceY[GUI_HW_WIIMOTE]) > 1.6 || (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_UP) || (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_2)) J |= VBA_BUTTON_L;

		if (hw & GUI_TRIGGER_L) J |= VBA_BUTTON_R;
		if ((hw & GUI_BTN_LEFT) || (hw & GUI_BTN_RIGHT)) J |= VBA_BUTTON_B;
		if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1) J |= VBA_SPEED;
	} else if (data.hw_connected[GUI_HW_CLASSIC]) {
		J |= StandardClassic(pad);
	} else {
		J |= StandardSideways(pad);
	}
	return J;
}

uint32_t MohUndergroundInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardClassic(pad) | StandardSideways(pad);
	static bool crouched = false;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		if (J & VBA_LEFT) J |= VBA_BUTTON_L;
		if (J & VBA_RIGHT) J |= VBA_BUTTON_R;
		J &= ~(VBA_LEFT | VBA_RIGHT);

		CursorVisible = true;
		if (data.validPointer) {
			if (data.cursor_x < 320 - 40) J |= VBA_LEFT;
			else if (data.cursor_x > 320 + 40) J |= VBA_RIGHT;
		}

		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		if (data.hw_buttons_d[GUI_HW_NUNCHUK] & GUI_BTN_DOWN) crouched = !crouched; // Toggle

		if (hw & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (hw & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (hw & GUI_BTN_B) J |= VBA_BUTTON_A;

		if (fabs(data.hw_gforceY[GUI_HW_WIIMOTE]) > 1.6 || (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_UP) || (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_2)) J |= VBA_BUTTON_SELECT;
		if ((hw & GUI_BTN_LEFT) || (hw & GUI_BTN_RIGHT)) J |= VBA_BUTTON_B;
		if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1) J |= VBA_SPEED;
	} else {
		CursorVisible = false;
	}

	if (crouched && (!(J & VBA_BUTTON_L)) && (!(J & VBA_BUTTON_R)))
		J |= VBA_BUTTON_L | VBA_BUTTON_R;

	return J;
}

uint32_t BoktaiInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardDPad(pad) | StandardClassic(pad);

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	static bool GunRaised = false;
	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;

	if ((-data.hw_pitch[GUI_HW_WIIMOTE]) > 45) {
		GunRaised = true;
	} else if ((-data.hw_pitch[GUI_HW_WIIMOTE]) < 40) {
		GunRaised = false;
	}

	if (GunRaised) J |= VBA_BUTTON_A;
	if (data.buttons_h & GUI_BTN_B) J |= VBA_BUTTON_B;
	if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_2) J |= VBA_BUTTON_R;
	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_L) J |= VBA_BUTTON_R;
		if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_ZL) J |= VBA_BUTTON_L;
		if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1) J |= VBA_SPEED;
	} else {
		if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1) J |= VBA_BUTTON_L;
	}
	return J;
}

uint32_t Boktai2Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardDPad(pad) | StandardClassic(pad);

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	static bool GunRaised = false;
	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;

	if ((-data.hw_pitch[GUI_HW_WIIMOTE]) > 45) {
		GunRaised = true;
	} else if ((-data.hw_pitch[GUI_HW_WIIMOTE]) < 40) {
		GunRaised = false;
	}

	if (GunRaised) J |= VBA_BUTTON_A;
	if (data.buttons_h & GUI_BTN_B) J |= VBA_BUTTON_B;

	if (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.8) J |= VBA_BUTTON_B;

	if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_2) J |= VBA_BUTTON_R;
	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_L) J |= VBA_BUTTON_R;
		if (data.hw_buttons_h[GUI_HW_NUNCHUK] & GUI_TRIGGER_ZL) J |= VBA_BUTTON_L;
		if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1) J |= VBA_SPEED;
	} else {
		if (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1) J |= VBA_BUTTON_L;
	}
	return J;
}

uint32_t OnePieceInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardSideways(pad) | StandardClassic(pad);
	static uint32_t LastDir = VBA_RIGHT;
	bool JumpButton=0, AttackButton=0, ViewButton=0, CharacterButton=0, PauseButton=0,
	DashButton=0, GrabButton=0, SpeedButton=0, AttackUpButton = 0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		J |= StandardDPad(pad);
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		JumpButton = hw & GUI_BTN_B;
		AttackButton = hw & GUI_BTN_A;
		CharacterButton = hw & GUI_BTN_MINUS;
		PauseButton = hw & GUI_BTN_PLUS;
		DashButton = hw & GUI_TRIGGER_L;
		GrabButton = hw & GUI_TRIGGER_ZL;
		ViewButton = data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1;
		SpeedButton = data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_2;
	}

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (data.hw_substickX[GUI_HW_GAMECUBE] > 0.55f) J |= VBA_SPEED;
		JumpButton = JumpButton || (gc & GUI_BTN_Y);
		AttackButton = AttackButton || (gc & GUI_BTN_A);
		GrabButton = GrabButton || (gc & GUI_BTN_B);
		AttackUpButton = AttackUpButton || (gc & GUI_BTN_X);
		DashButton = DashButton || (gc & GUI_TRIGGER_L);
		PauseButton = PauseButton || (gc & GUI_BTN_PLUS);
		CharacterButton = CharacterButton || (gc & GUI_TRIGGER_R);
	}
	
	if (JumpButton) J |= VBA_BUTTON_A;
	if (AttackButton) J |= VBA_BUTTON_B;
	if (AttackUpButton) J |= VBA_UP | VBA_BUTTON_B;
	if (CharacterButton) J |= VBA_BUTTON_L;
	if (DashButton) J |= LastDir;
	if (PauseButton) J |= VBA_BUTTON_START;
	if (GrabButton) J |= VBA_BUTTON_R;
	if (SpeedButton) J |= VBA_SPEED;
	if (ViewButton) J |= VBA_BUTTON_SELECT;

	if (J & VBA_RIGHT) LastDir = VBA_RIGHT;
	else if (J & VBA_LEFT) LastDir = VBA_LEFT;
	
	return J;
}

uint32_t HobbitInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardSideways(pad) | StandardClassic(pad);

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	bool AbilityButton=0, AttackButton=0, UseButton=0, ChangeSkillButton=0, PauseButton=0, ItemsButton=0, SpeedButton=0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		J |= StandardDPad(pad);
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		AbilityButton = hw & GUI_BTN_B;
		UseButton = hw & GUI_BTN_A;
		PauseButton = hw & GUI_BTN_PLUS;
		ItemsButton = hw & GUI_BTN_MINUS;
		SpeedButton = hw & GUI_TRIGGER_L;
		ChangeSkillButton = hw & GUI_TRIGGER_ZL;
		AttackButton = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5);
	}
	
	if (AbilityButton) J |= VBA_BUTTON_B;
	if (AttackButton) J |= VBA_BUTTON_L;
	if (ChangeSkillButton) J |= VBA_BUTTON_L;
	if (ItemsButton) J |= VBA_BUTTON_SELECT;
	if (UseButton) J |= VBA_BUTTON_A;
	if (SpeedButton) J |= VBA_BUTTON_R;
	if (PauseButton) J |= VBA_BUTTON_START;

	return J;
}

uint32_t FellowshipOfTheRingInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardSideways(pad) | StandardClassic(pad);

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	bool CancelButton=0, UseButton=0, ChangeCharButton=0, PauseButton=0, ItemsButton=0, SpeedButton=0, SelectButton=0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		J |= StandardDPad(pad);
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		CancelButton = hw & GUI_BTN_B;
		UseButton = hw & GUI_BTN_A;
		PauseButton = hw & GUI_BTN_PLUS;
		ItemsButton = hw & GUI_BTN_MINUS;
		SpeedButton = hw & GUI_TRIGGER_L;
		ChangeCharButton = hw & GUI_TRIGGER_ZL;
		CancelButton = CancelButton || (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5);
		SelectButton = data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1;
	}

	if (UseButton) J |= VBA_BUTTON_A;
	if (CancelButton) J |= VBA_BUTTON_B;
	if (ChangeCharButton) J |= VBA_BUTTON_L;
	if (ItemsButton) J |= VBA_BUTTON_R;
	if (SpeedButton) J |= VBA_SPEED;
	if (PauseButton) J |= VBA_BUTTON_START;
	if (SelectButton) J |= VBA_BUTTON_SELECT;

	return J;
}

uint32_t ReturnOfTheKingInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardSideways(pad) | StandardClassic(pad);

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	bool AbilityButton=0, AttackButton=0, UseButton=0, ChangeSkillButton=0, PauseButton=0, ItemsButton=0, SpeedButton=0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		J |= StandardDPad(pad);
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		AbilityButton = hw & GUI_BTN_B;
		UseButton = hw & GUI_BTN_A;
		PauseButton = hw & GUI_BTN_PLUS;
		ItemsButton = hw & GUI_BTN_MINUS;
		SpeedButton = hw & GUI_TRIGGER_L;
		ChangeSkillButton = hw & GUI_TRIGGER_ZL;
		AttackButton = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5);
	}
	
	if (AbilityButton) J |= VBA_BUTTON_A;
	if (AttackButton) J |= VBA_BUTTON_B;
	if (ChangeSkillButton) J |= VBA_BUTTON_L;
	if (ItemsButton) J |= VBA_BUTTON_START;
	if (UseButton) J |= VBA_BUTTON_R;
	if (SpeedButton) J |= VBA_SPEED;
	if (PauseButton) J |= VBA_BUTTON_SELECT;

	return J;
}

uint32_t CastlevaniaAdventureInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardSideways(pad) | StandardClassic(pad);

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	bool JumpButton=0, AttackButton=0, GuardButton=0, PauseButton=0, SelectButton=0, SpeedButton=0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		AttackButton = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5) || (fabs(data.hw_gforceY[GUI_HW_WIIMOTE]) > 1.5) || (hw & GUI_BTN_B);
		JumpButton = hw & GUI_TRIGGER_L;
		GuardButton = hw & GUI_TRIGGER_ZL;
		PauseButton = hw & GUI_BTN_PLUS;
		SelectButton = hw & GUI_BTN_MINUS;
		SpeedButton = (hw & GUI_BTN_A) && (hw & GUI_BTN_B);
	} else if (data.hw_connected[GUI_HW_CLASSIC]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_CLASSIC];
		AttackButton = hw & GUI_BTN_B;
		JumpButton = hw & GUI_TRIGGER_R;
		GuardButton = hw & GUI_TRIGGER_L;
		PauseButton = hw & GUI_BTN_PLUS;
		SelectButton = hw & GUI_BTN_MINUS;
		SpeedButton = hw & GUI_BTN_A;
	}
	
	if (JumpButton) J |= VBA_BUTTON_A;
	if (AttackButton) J |= VBA_BUTTON_B;
	if (PauseButton) J |= VBA_BUTTON_START;
	if (SelectButton) J |= VBA_BUTTON_SELECT;
	if (SpeedButton) J |= VBA_SPEED;
	if (GuardButton) {
		J &= ~VBA_UP;
		J |= VBA_DOWN;
	}

	return J;
}

uint32_t CastlevaniaBelmontInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardSideways(pad) | StandardClassic(pad);

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	bool JumpButton=0, AttackButton=0, ShootButton=0, GuardButton=0, PauseButton=0, SelectButton=0, SpeedButton=0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		ShootButton = hw & GUI_BTN_A;
		AttackButton = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5) || (fabs(data.hw_gforceY[GUI_HW_WIIMOTE]) > 1.5) || (hw & GUI_BTN_B);
		JumpButton = hw & GUI_TRIGGER_L;
		GuardButton = hw & GUI_TRIGGER_ZL;
		PauseButton = hw & GUI_BTN_PLUS;
		SelectButton = hw & GUI_BTN_MINUS;
		SpeedButton = (hw & GUI_BTN_A) && (hw & GUI_BTN_B);
	} else if (data.hw_connected[GUI_HW_CLASSIC]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_CLASSIC];
		ShootButton = hw & GUI_BTN_Y;
		AttackButton = hw & GUI_BTN_B;
		JumpButton = hw & GUI_TRIGGER_R;
		GuardButton = hw & GUI_TRIGGER_L;
		PauseButton = hw & GUI_BTN_PLUS;
		SelectButton = hw & GUI_BTN_MINUS;
		SpeedButton = hw & GUI_BTN_A;
	}
	
	if (JumpButton) J |= VBA_BUTTON_A;
	if (AttackButton) {
		J &= ~VBA_UP;
		J |= VBA_BUTTON_B;
	}
	if (ShootButton) J |= VBA_UP | VBA_BUTTON_B;
	if (PauseButton) J |= VBA_BUTTON_START;
	if (SelectButton) J |= VBA_BUTTON_SELECT;
	if (SpeedButton) J |= VBA_SPEED;
	if (GuardButton) {
		J &= ~VBA_UP;
		J |= VBA_DOWN;
	}

	return J;
}

uint32_t CastlevaniaLegendsInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardSideways(pad) | StandardClassic(pad);

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	bool JumpButton=0, AttackButton=0, ShootButton=0, GuardButton=0, PauseButton=0, SelectButton=0, SpeedButton=0, HyperButton=0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		ShootButton = hw & GUI_BTN_A;
		AttackButton = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5) || (fabs(data.hw_gforceY[GUI_HW_WIIMOTE]) > 1.5) || (hw & GUI_BTN_B);
		JumpButton = hw & GUI_TRIGGER_L;
		GuardButton = hw & GUI_TRIGGER_ZL;
		PauseButton = hw & GUI_BTN_PLUS;
		SelectButton = hw & GUI_BTN_MINUS;
		SpeedButton = (hw & GUI_BTN_A) && (hw & GUI_BTN_B);
		HyperButton = hw & GUI_BTN_DOWN;
	} else if (data.hw_connected[GUI_HW_CLASSIC]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_CLASSIC];
		ShootButton = hw & GUI_BTN_Y;
		AttackButton = hw & GUI_BTN_B;
		JumpButton = hw & GUI_TRIGGER_R;
		GuardButton = hw & GUI_TRIGGER_L;
		PauseButton = hw & GUI_BTN_PLUS;
		SelectButton = hw & GUI_BTN_MINUS;
		SpeedButton = hw & GUI_BTN_A;
		HyperButton = hw & GUI_BTN_X;
	}
	
	if (JumpButton) J |= VBA_BUTTON_A;
	if (AttackButton) {
		J &= ~VBA_UP;
		J |= VBA_BUTTON_B;
	}
	if (HyperButton) J |= VBA_BUTTON_A | VBA_BUTTON_B;
	if (ShootButton) J |= VBA_UP | VBA_BUTTON_B;
	if (PauseButton) J |= VBA_BUTTON_START;
	if (SelectButton) J |= VBA_BUTTON_SELECT;
	if (SpeedButton) J |= VBA_SPEED;
	if (GuardButton) {
		J &= ~VBA_UP;
		J |= VBA_DOWN;
	}

	return J;
}

uint32_t CastlevaniaCircleMoonInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardSideways(pad) | StandardClassic(pad);

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	bool JumpButton=0, AttackButton=0, ShootButton=0, GuardButton=0, PauseButton=0, SelectButton=0, SpeedButton=0, HyperButton=0,
	LButton=0, RButton=0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		ShootButton = hw & GUI_BTN_A;
		AttackButton = (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5) || (fabs(data.hw_gforceY[GUI_HW_WIIMOTE]) > 1.5) || (hw & GUI_BTN_B);
		JumpButton = hw & GUI_TRIGGER_L;
		GuardButton = hw & GUI_TRIGGER_ZL;
		PauseButton = hw & GUI_BTN_PLUS;
		SelectButton = hw & GUI_BTN_MINUS;
		SpeedButton = (hw & GUI_BTN_A) && (hw & GUI_BTN_B);
		HyperButton = hw & GUI_BTN_DOWN;
		LButton = data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1;
		RButton = data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_2;
	} else if (data.hw_connected[GUI_HW_CLASSIC]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_CLASSIC];
		ShootButton = hw & GUI_BTN_Y;
		AttackButton = hw & GUI_BTN_B;
		JumpButton = hw & GUI_TRIGGER_R;
		GuardButton = hw & GUI_TRIGGER_L;
		PauseButton = hw & GUI_BTN_PLUS;
		SelectButton = hw & GUI_BTN_MINUS;
		SpeedButton = hw & GUI_BTN_A;
		HyperButton = hw & GUI_BTN_X;
		// In driver layer, FULL_L and ZL are merged to GUI_TRIGGER_L, FULL_R and ZR to GUI_TRIGGER_R
		LButton = hw & GUI_TRIGGER_L;
		RButton = hw & GUI_TRIGGER_R;
	}
	
	if (JumpButton) J |= VBA_BUTTON_A;
	if (AttackButton) {
		J &= ~VBA_UP;
		J |= VBA_BUTTON_B;
	}
	if (HyperButton) J |= VBA_BUTTON_A | VBA_BUTTON_B;
	if (ShootButton) J |= VBA_UP | VBA_BUTTON_B;
	if (PauseButton) J |= VBA_BUTTON_START;
	if (SelectButton) J |= VBA_BUTTON_SELECT;
	if (SpeedButton) J |= VBA_SPEED;
	if (GuardButton) {
		J &= ~VBA_UP;
		J |= VBA_DOWN;
	}
	if (LButton) J |= VBA_BUTTON_L;
	if (RButton) J |= VBA_BUTTON_R;
	return J;
}

uint8_t KD_NOR[64] = {
0x7f, 0x00, 0x98, 0x67, 0x00, 0x99, 0x00, 0x89, 0x00, 0xa1, 0x00, 0xb1, 0x10, 0xa9, 0x18, 0x67,
0xff, 0x00, 0x81, 0x7e, 0x00, 0x81, 0x00, 0xb9, 0x10, 0xa9, 0x00, 0xb9, 0x00, 0x81, 0x81, 0x7e,
0xfe, 0x00, 0x03, 0xfc, 0x01, 0x82, 0x00, 0xb9, 0x00, 0xb9, 0x01, 0x82, 0x00, 0xb9, 0x10, 0xef,
0x00, 0x00, 0x00, 0x20, 0x00, 0x70, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t KD_BAT[64] = {
0x7f, 0x00, 0x83, 0x7c, 0x01, 0x82, 0x00, 0xb9, 0x01, 0x82, 0x00, 0xb9, 0x00, 0x81, 0x01, 0x7e,
0xff, 0x00, 0x81, 0x7e, 0x00, 0x81, 0x00, 0xb9, 0x00, 0xb9, 0x00, 0x81, 0x00, 0xb9, 0x10, 0xef,
0xfe, 0x00, 0x00, 0xff, 0x00, 0x81, 0x00, 0xe7, 0xc3, 0x24, 0xc3, 0x24, 0xc3, 0x24, 0xc2, 0x3c,
0x00, 0x00, 0x00, 0x20, 0x00, 0x70, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t KD_ACTUAL[64];

uint8_t KD_NOR_GRAPHICS[128] = {
0x00, 0x00, 0x00, 0x0f, 0x04, 0x07, 0x18, 0x1f, 0x0d, 0x0e, 0x40, 0x7f, 0x18, 0x1f, 0x70, 0x7f,
0xc0, 0xff, 0x70, 0x7f, 0x01, 0x3e, 0x0c, 0x0f, 0x3e, 0x3f, 0x10, 0x1f, 0x06, 0x07, 0x00, 0x00,
0x00, 0x00, 0x18, 0xe0, 0xe4, 0x18, 0x3a, 0xc4, 0xfc, 0x02, 0x3d, 0xc2, 0xfe, 0x01, 0x3e, 0xc1,
0x7e, 0x81, 0x3e, 0xc1, 0xfd, 0x02, 0x7c, 0x82, 0x1a, 0xe4, 0xe4, 0x18, 0x10, 0xe0, 0x00, 0x00,
0x04, 0x03, 0x13, 0x0c, 0x2f, 0x10, 0x5f, 0x20, 0x5f, 0x20, 0x3f, 0x40, 0x2a, 0x55, 0x2a, 0x55,
0x08, 0x77, 0x00, 0x7f, 0x28, 0x7f, 0x1a, 0x5f, 0x13, 0x17, 0x01, 0x05, 0x05, 0x05, 0x00, 0x00,
0x20, 0xc0, 0xc8, 0x30, 0xf4, 0x08, 0xf8, 0x04, 0xfa, 0x04, 0xf4, 0x0a, 0xb4, 0x4a, 0x24, 0xda,
0x20, 0xde, 0x0a, 0xfe, 0x1a, 0xfe, 0x18, 0xfc, 0x4c, 0xec, 0x48, 0xe8, 0xc0, 0xc0, 0x80, 0x80};
uint8_t KD_ACTUAL_GRAPHICS[128];

bool KD_WeaponPressed = false;
s8 KD_LastWeapon; // -1 for selected weapon, 0 for NOR, 4 for BAT
uint8_t KD_ActualItem; // Item selected by the player (byte in memory)

void KD_WeaponToMemory() {
	// If the fourth 8x8 tile of the weapon indicator doesn't contain the "+" icon I made, the weapon has been changed in the game and has to be updated in memory.
	bool hasPlusIcon = true;
	for (int i=48; i<64; i++) {
		if (gbReadMemory(0x9110+i) != KD_BAT[i]) {
			hasPlusIcon = false;
			break;
		}
	}
	if (hasPlusIcon) return;
	
	KD_ActualItem = gbReadMemory(0xC8CB);
	for (int i=0; i<64; i++) {
		KD_ACTUAL[i] = gbReadMemory(0x9110+i);
	}
	for (int i=0; i<128; i++) {
		KD_ACTUAL_GRAPHICS[i] = gbReadMemory(0x8f40+i);
	}
}

uint32_t KidDraculaInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	uint32_t J = StandardMovement(pad) | StandardSideways(pad) | StandardClassic(pad);

	if (data.hw_connected[GUI_HW_GAMECUBE]) {
		uint32_t gc = data.hw_buttons_h[GUI_HW_GAMECUBE];
		if (gc & GUI_BTN_A) J |= VBA_BUTTON_A;
		if (gc & GUI_BTN_B) J |= VBA_BUTTON_B;
		if (gc & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
		if (gc & GUI_TRIGGER_ZR) J |= VBA_BUTTON_SELECT;
		if (gc & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
		if (gc & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	}

	bool JumpButton=0, ShootButton=0, PauseButton=0, SelectButton=0, SpeedButton=0, NorButton=0, BatButton=0;

	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_NUNCHUK];
		JumpButton = hw & GUI_BTN_A;
		ShootButton = hw & GUI_BTN_B;
		PauseButton = hw & GUI_BTN_PLUS;
		SelectButton = hw & GUI_BTN_MINUS;
		SpeedButton = (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_1) || (data.hw_buttons_h[GUI_HW_WIIMOTE] & GUI_BTN_2);
		NorButton = hw & GUI_TRIGGER_ZL;
		BatButton = hw & GUI_TRIGGER_L;
	} else if (data.hw_connected[GUI_HW_CLASSIC]) {
		uint32_t hw = data.hw_buttons_h[GUI_HW_CLASSIC];
		JumpButton = hw & (GUI_BTN_B | GUI_BTN_A);
		ShootButton = hw & GUI_BTN_Y;
		PauseButton = hw & GUI_BTN_PLUS;
		SelectButton = hw & GUI_BTN_MINUS;
		SpeedButton = hw & GUI_TRIGGER_L;
		NorButton = hw & GUI_BTN_X;
		BatButton = hw & GUI_TRIGGER_R;
	}

	if (JumpButton) J |= VBA_BUTTON_A;
	if (ShootButton && !(KD_WeaponPressed && KD_LastWeapon != -1)) {
		J |= VBA_BUTTON_B;
		KD_LastWeapon = -1;
		// Insert original weapon and graphics
		KD_WeaponToMemory();
		gbWriteMemory(0xC8CB, KD_ActualItem);
		for (int i=0; i<64; i++) {
			gbWriteMemory(0x9110+i, KD_ACTUAL[i]);
		}
		for (int i=0; i<128; i++) {
			gbWriteMemory(0x8f40+i, KD_ACTUAL_GRAPHICS[i]);
		}
	}
	if (NorButton && !(KD_WeaponPressed && KD_LastWeapon != 0)) {
		J |= VBA_BUTTON_B;
		KD_LastWeapon = 0;
		// Insert NOR weapon and graphics
		KD_WeaponToMemory();
		gbWriteMemory(0xC8CB, 0);
		for (int i=0; i<64; i++) {
			gbWriteMemory(0x9110+i, KD_NOR[i]);
		}
		for (int i=0; i<128; i++) {
			gbWriteMemory(0x8f40+i, KD_NOR_GRAPHICS[i]);
		}
	}
	if (BatButton && !(KD_WeaponPressed && KD_LastWeapon != 4)) {
		J |= VBA_BUTTON_B;
		KD_LastWeapon = 4;
		// Insert BAT weapon and graphics
		KD_WeaponToMemory();
		gbWriteMemory(0xC8CB, 4);
		for (int i=0; i<64; i++) {
			gbWriteMemory(0x9110+i, KD_BAT[i]);
		}
	}
	if (PauseButton) J |= VBA_BUTTON_START;
	if (SelectButton) J |= VBA_BUTTON_SELECT;
	if (SpeedButton) J |= VBA_SPEED;
	KD_WeaponPressed = (ShootButton || NorButton || BatButton);
	return J;
}

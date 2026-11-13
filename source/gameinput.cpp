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
#include "drivers/InputController.h"
#include "drivers/Platform.h"

#include "vba/gba/GBA.h"
#include "vba/gba/bios.h"
#include "vba/gba/GBAinline.h"
#include "legacyinput.h"

extern bool CalibrateWario;

char DebugStr[50] = "";

void DebugPrintf(const char *format, ...) {
	va_list args;
	va_start( args, format );
	vsprintf( DebugStr, format, args );
	va_end( args );
}

uint32_t TMNTInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | StandardDPad(pad);
	static uint32_t LastDir = VBA_RIGHT;
	static bool wait = false;
	static int holdcount = 0;
	bool Jump=0, Attack=0, SpinKick=0, Roll=0, Pause=0, Select=0;

	const LegacyWpad * wp = LegacyWpad_Data(pad);
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		Jump = (wp->btns_h & LW_BUTTON_A);
		Attack = (fabs(wp->gforce.x)> 1.5);
		SpinKick = (fabs(wp->exp.nunchuk.gforce.x)> 0.5);
		Roll = (wp->btns_h & LW_NUNCHUK_BUTTON_Z || wp->btns_h & LW_NUNCHUK_BUTTON_C);
		Pause = (wp->btns_h & LW_BUTTON_PLUS);
		Select = (wp->btns_h & LW_BUTTON_MINUS);
		// Swap Turtles or super turtle summon
		if (wp->btns_h & LW_BUTTON_B) {
			if (wp->exp.nunchuk.orient.pitch < -35 && wp->orient.pitch < -35)
				J |= VBA_BUTTON_L | VBA_BUTTON_R;
			else J |= VBA_BUTTON_R;
		}
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		Jump = (wp->btns_h & LW_CLASSIC_BUTTON_B);
		Attack = (wp->btns_h & LW_CLASSIC_BUTTON_A);
		SpinKick = (wp->btns_h & LW_CLASSIC_BUTTON_X);
		Pause = (wp->btns_h & LW_CLASSIC_BUTTON_PLUS);
		Select = (wp->btns_h & LW_CLASSIC_BUTTON_MINUS);
		Roll = (wp->btns_h & (LW_CLASSIC_BUTTON_FULL_L | LW_CLASSIC_BUTTON_FULL_R | LW_CLASSIC_BUTTON_ZL | LW_CLASSIC_BUTTON_ZR));
		// Swap Turtles or super turtle summon
		if (wp->btns_h & LW_CLASSIC_BUTTON_Y) {
			holdcount++;
			if (holdcount > 20)
				J |= VBA_BUTTON_L | VBA_BUTTON_R;
		}
		if (wp->btns_u & LW_CLASSIC_BUTTON_Y) {
			if (holdcount <= 20)
				J |= VBA_BUTTON_R;
			holdcount = 0;
		}
	} else {
		Jump = (wp->btns_h & LW_BUTTON_A);
		Attack = (fabs(wp->gforce.x)> 1.5);
		Pause = (wp->btns_h & LW_BUTTON_PLUS);
		Select = (wp->btns_h & LW_BUTTON_MINUS);
		// Swap Turtles or super turtle summon
		if (wp->btns_h & LW_BUTTON_B) {
			if (wp->orient.pitch < -40)
				J |= VBA_BUTTON_L | VBA_BUTTON_R;
			else J |= VBA_BUTTON_R;
		}
		SpinKick = (wp->btns_h & LW_BUTTON_1);
		Roll = (wp->btns_h & LW_BUTTON_2);
	}

	uint32_t gc = LegacyPad_ButtonsHeld(pad);
	uint32_t released = LegacyPad_ButtonsUp(pad);
	// DPad moves
	if (gc & LP_BUTTON_UP)
		J |= VBA_UP;
	if (gc & LP_BUTTON_DOWN)
		J |= VBA_DOWN;
	if (gc & LP_BUTTON_LEFT)
		J |= VBA_LEFT;
	if (gc & LP_BUTTON_RIGHT)
		J |= VBA_RIGHT;
	// Jump
	if (gc & LP_BUTTON_A) J |= VBA_BUTTON_A;
	// Swap turtles, hold for super family move
	if (gc & LP_BUTTON_B) {
		holdcount++;
		if (holdcount > 20)
			J |= VBA_BUTTON_L | VBA_BUTTON_R;
	}
	if (released & LP_BUTTON_B) {
		if (holdcount <= 20)
			J |= VBA_BUTTON_R;
		holdcount = 0;
	}
	// Attack
	if (gc & LP_BUTTON_X) Attack = true;
	// Spin kick
	if (gc & LP_BUTTON_Y) SpinKick = true;
	// Pause
	if (gc & LP_BUTTON_START) Pause = true;
	// Select
	if (gc & LP_TRIGGER_Z) Select = true;
	// Roll
	if (gc & LP_TRIGGER_L || gc & LP_TRIGGER_R) Roll = true;

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
	uint32_t J = StandardMovement(pad) | StandardDPad(pad);
	bool Jump=0, Attack=0, SpinKick=0, Pause=0, Select=0;

	const LegacyWpad * wp = LegacyWpad_Data(pad);
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		Jump = (wp->btns_h & LW_BUTTON_A);
		Attack = (fabs(wp->gforce.x)> 1.5);
		SpinKick = (fabs(wp->exp.nunchuk.gforce.x)> 0.5);
		Pause = (wp->btns_h & LW_BUTTON_PLUS);
		Select = (wp->btns_h & LW_BUTTON_MINUS);
		// Swap Turtles or super turtle summon
		if (wp->btns_h & LW_BUTTON_B) {
			// N/A
		}
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		J |= StandardDPad(pad);
		Jump = (wp->btns_h & LW_CLASSIC_BUTTON_B);
		Attack = (wp->btns_h & LW_CLASSIC_BUTTON_A);
		SpinKick = (wp->btns_h & LW_CLASSIC_BUTTON_X);
		Pause = (wp->btns_h & LW_CLASSIC_BUTTON_PLUS);
		Select = (wp->btns_h & LW_CLASSIC_BUTTON_MINUS);
		// Swap Turtles or super turtle summon
		if (wp->btns_h & LW_CLASSIC_BUTTON_Y) {
			// N/A
		}
	} else {
		Jump = (wp->btns_h & LW_BUTTON_A);
		Attack = (fabs(wp->gforce.x)> 1.5);
		Pause = (wp->btns_h & LW_BUTTON_PLUS);
		Select = (wp->btns_h & LW_BUTTON_MINUS);
		// Swap Turtles or super turtle summon
		if (wp->btns_h & LW_BUTTON_B) {
			//N/A
		}
		SpinKick = (wp->btns_h & LW_BUTTON_1);
	}

	uint32_t gc = LegacyPad_ButtonsHeld(pad);
	// DPad moves
	if (gc & LP_BUTTON_UP)
		J |= VBA_UP;
	if (gc & LP_BUTTON_DOWN)
		J |= VBA_DOWN;
	if (gc & LP_BUTTON_LEFT)
		J |= VBA_LEFT;
	if (gc & LP_BUTTON_RIGHT)
		J |= VBA_RIGHT;
	// Jump
	if (gc & LP_BUTTON_A) J |= VBA_BUTTON_A;
	// Swap turtles, hold for super family move
	if (gc & LP_BUTTON_B) {
		// N/A
	}
	// Attack
	if (gc & LP_BUTTON_X) Attack = true;
	// Spin kick
	if (gc & LP_BUTTON_Y) SpinKick = true;
	// Pause
	if (gc & LP_BUTTON_START) Pause = true;
	// Select
	if (gc & LP_TRIGGER_Z) Select = true;

	if (Jump) J |= VBA_BUTTON_A;
	if (Attack || SpinKick) J |= VBA_BUTTON_B;
	if (Pause) J |= VBA_BUTTON_START;
	if (Select) J |= VBA_BUTTON_SELECT;
		
	return J;
}

uint32_t TMNT2Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | StandardDPad(pad);
	bool Jump=0, Attack=0, SpinKick=0, Roll=0, Pause=0, Select=0;

	const LegacyWpad * wp = LegacyWpad_Data(pad);
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		Jump = (wp->btns_h & LW_BUTTON_A);
		Attack = (fabs(wp->gforce.x)> 1.5);
		SpinKick = (fabs(wp->exp.nunchuk.gforce.x)> 0.5);
		Roll = (wp->btns_h & LW_NUNCHUK_BUTTON_Z || wp->btns_h & LW_NUNCHUK_BUTTON_C);
		Pause = (wp->btns_h & LW_BUTTON_PLUS);
		Select = (wp->btns_h & LW_BUTTON_MINUS);
		// Swap Turtles or super turtle summon
		if (wp->btns_h & LW_BUTTON_B) {
			// N/A
		}
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		J |= StandardDPad(pad);
		Jump = (wp->btns_h & LW_CLASSIC_BUTTON_B);
		Attack = (wp->btns_h & LW_CLASSIC_BUTTON_A);
		SpinKick = (wp->btns_h & LW_CLASSIC_BUTTON_X);
		Pause = (wp->btns_h & LW_CLASSIC_BUTTON_PLUS);
		Select = (wp->btns_h & LW_CLASSIC_BUTTON_MINUS);
		Roll = (wp->btns_h & (LW_CLASSIC_BUTTON_FULL_L | LW_CLASSIC_BUTTON_FULL_R | LW_CLASSIC_BUTTON_ZL | LW_CLASSIC_BUTTON_ZR));
		// Swap Turtles or super turtle summon
		if (wp->btns_h & LW_CLASSIC_BUTTON_Y) {
			// N/A
		}
	} else {
		Jump = (wp->btns_h & LW_BUTTON_A);
		Attack = (fabs(wp->gforce.x)> 1.5);
		Pause = (wp->btns_h & LW_BUTTON_PLUS);
		Select = (wp->btns_h & LW_BUTTON_MINUS);
		// Swap Turtles or super turtle summon
		if (wp->btns_h & LW_BUTTON_B) {
			//N/A
		}
		SpinKick = (wp->btns_h & LW_BUTTON_1);
		Roll = (wp->btns_h & LW_BUTTON_2);
	}

	uint32_t gc = LegacyPad_ButtonsHeld(pad);
	// DPad moves
	if (gc & LP_BUTTON_UP)
		J |= VBA_UP;
	if (gc & LP_BUTTON_DOWN)
		J |= VBA_DOWN;
	if (gc & LP_BUTTON_LEFT)
		J |= VBA_LEFT;
	if (gc & LP_BUTTON_RIGHT)
		J |= VBA_RIGHT;
	// Jump
	if (gc & LP_BUTTON_A) J |= VBA_BUTTON_A;
	// Swap turtles, hold for super family move
	if (gc & LP_BUTTON_B) {
		// N/A
	}
	// Attack
	if (gc & LP_BUTTON_X) Attack = true;
	// Spin kick
	if (gc & LP_BUTTON_Y) SpinKick = true;
	// Pause
	if (gc & LP_BUTTON_START) Pause = true;
	// Select
	if (gc & LP_TRIGGER_Z) Select = true;
	// Roll
	if (gc & LP_TRIGGER_L || gc & LP_TRIGGER_R) Roll = true;

	if (Jump) J |= VBA_BUTTON_A;
	if (Attack) J |= VBA_BUTTON_B;
	if (SpinKick || Roll) J |= VBA_BUTTON_B | VBA_BUTTON_A;
	if (Pause) J |= VBA_BUTTON_START;
	if (Select) J |= VBA_BUTTON_SELECT;
		
	return J;
}

uint32_t TMNT3Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | StandardDPad(pad);
	bool Jump=0, Attack=0, SpinKick=0, Roll=0, Pause=0, Select=0;

	const LegacyWpad * wp = LegacyWpad_Data(pad);
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		Jump = (wp->btns_h & LW_BUTTON_A);
		Attack = (fabs(wp->gforce.x)> 1.5);
		SpinKick = (fabs(wp->exp.nunchuk.gforce.x)> 0.5);
		Roll = (wp->btns_h & LW_NUNCHUK_BUTTON_Z || wp->btns_h & LW_NUNCHUK_BUTTON_C);
		Pause = (wp->btns_h & LW_BUTTON_PLUS);
		Select = (wp->btns_h & LW_BUTTON_MINUS);
		// Swap Turtles or super turtle summon
		if (wp->btns_h & LW_BUTTON_B) {
			J |= VBA_BUTTON_START;
		}
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		J |= StandardDPad(pad);
		Jump = (wp->btns_h & LW_CLASSIC_BUTTON_B);
		Attack = (wp->btns_h & LW_CLASSIC_BUTTON_A);
		SpinKick = (wp->btns_h & LW_CLASSIC_BUTTON_X);
		Pause = (wp->btns_h & LW_CLASSIC_BUTTON_PLUS);
		Select = (wp->btns_h & LW_CLASSIC_BUTTON_MINUS);
		Roll = (wp->btns_h & (LW_CLASSIC_BUTTON_FULL_L | LW_CLASSIC_BUTTON_FULL_R | LW_CLASSIC_BUTTON_ZL | LW_CLASSIC_BUTTON_ZR));
		// Swap Turtles or super turtle summon
		if (wp->btns_h & LW_CLASSIC_BUTTON_Y) {
			J |= VBA_BUTTON_START;
		}
	} else {
		Jump = (wp->btns_h & LW_BUTTON_A);
		Attack = (fabs(wp->gforce.x)> 1.5);
		Pause = (wp->btns_h & LW_BUTTON_PLUS);
		Select = (wp->btns_h & LW_BUTTON_MINUS);
		// Swap Turtles or super turtle summon
		if (wp->btns_h & LW_BUTTON_B) {
			J |= VBA_BUTTON_START;
		}
		SpinKick = (wp->btns_h & LW_BUTTON_1);
		Roll = (wp->btns_h & LW_BUTTON_2);
	}

	uint32_t gc = LegacyPad_ButtonsHeld(pad);
	// DPad moves
	if (gc & LP_BUTTON_UP)
		J |= VBA_UP;
	if (gc & LP_BUTTON_DOWN)
		J |= VBA_DOWN;
	if (gc & LP_BUTTON_LEFT)
		J |= VBA_LEFT;
	if (gc & LP_BUTTON_RIGHT)
		J |= VBA_RIGHT;
	// Jump
	if (gc & LP_BUTTON_A) J |= VBA_BUTTON_A;
	// Swap turtles
	if (gc & LP_BUTTON_B) {
		J |= VBA_BUTTON_START;
	}
	// Attack
	if (gc & LP_BUTTON_X) Attack = true;
	// Spin kick
	if (gc & LP_BUTTON_Y) SpinKick = true;
	// Pause
	if (gc & LP_BUTTON_START) Pause = true;
	// Select
	if (gc & LP_TRIGGER_Z) Select = true;
	// Roll
	if (gc & LP_TRIGGER_L || gc & LP_TRIGGER_R) Roll = true;

	if (Jump || Roll) J |= VBA_BUTTON_A;
	if (Attack || SpinKick) J |= VBA_BUTTON_B;
	if (Pause) J |= VBA_BUTTON_SELECT;
	if (Select) J |= VBA_BUTTON_START;
		
	return J;
}

uint32_t TMNTGBAInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | StandardDPad(pad);
	bool Jump=0, Attack=0, SpinKick=0, SpecialMove=0, Pause=0, Select=0;

	const LegacyWpad * wp = LegacyWpad_Data(pad);
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		Jump = (wp->btns_h & LW_BUTTON_A);
		Attack = (fabs(wp->gforce.x)> 1.5) || (wp->btns_h & LW_BUTTON_B);
		SpinKick = (fabs(wp->exp.nunchuk.gforce.x)> 0.5);
		SpecialMove = (wp->btns_h & LW_NUNCHUK_BUTTON_Z);
		Pause = (wp->btns_h & LW_BUTTON_PLUS);
		Select = (wp->btns_h & LW_BUTTON_MINUS);
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		Jump = (wp->btns_h & LW_CLASSIC_BUTTON_B);
		Attack = (wp->btns_h & LW_CLASSIC_BUTTON_A);
		SpinKick = (wp->btns_h & LW_CLASSIC_BUTTON_X);
		Pause = (wp->btns_h & LW_CLASSIC_BUTTON_PLUS);
		Select = (wp->btns_h & LW_CLASSIC_BUTTON_MINUS);
		SpecialMove = (wp->btns_h & (LW_CLASSIC_BUTTON_FULL_L | LW_CLASSIC_BUTTON_FULL_R | LW_CLASSIC_BUTTON_ZL | LW_CLASSIC_BUTTON_ZR));
	} else {
		Jump = (wp->btns_h & LW_BUTTON_A);
		Attack = (fabs(wp->gforce.x)> 1.5) || (wp->btns_h & LW_BUTTON_B);
		Pause = (wp->btns_h & LW_BUTTON_PLUS);
		Select = (wp->btns_h & LW_BUTTON_MINUS);
		SpinKick = (wp->btns_h & LW_BUTTON_1);
		SpecialMove = (wp->btns_h & LW_BUTTON_2);
	}

	uint32_t gc = LegacyPad_ButtonsHeld(pad);
	// DPad moves
	if (gc & LP_BUTTON_UP)
		J |= VBA_UP;
	if (gc & LP_BUTTON_DOWN)
		J |= VBA_DOWN;
	if (gc & LP_BUTTON_LEFT)
		J |= VBA_LEFT;
	if (gc & LP_BUTTON_RIGHT)
		J |= VBA_RIGHT;
	// Jump
	if (gc & LP_BUTTON_A) J |= VBA_BUTTON_A;
	// Attack
	if (gc & LP_BUTTON_X) Attack = true;
	// Spin kick
	if (gc & LP_BUTTON_Y) SpinKick = true;
	// Pause
	if (gc & LP_BUTTON_START) Pause = true;
	// Select
	if (gc & LP_TRIGGER_Z) Select = true;
	// SpecialMove
	if (gc & LP_TRIGGER_L || gc & LP_TRIGGER_R) SpecialMove = true;

	if (Jump) J |= VBA_BUTTON_A;
	if (Attack) J |= VBA_BUTTON_B;
	if (SpinKick) J |= VBA_BUTTON_R;
	if (Pause) J |= VBA_BUTTON_START;
	if (Select) J |= VBA_BUTTON_SELECT;
	if (SpecialMove) {
		J |= VBA_BUTTON_R | VBA_BUTTON_A; // CAKTODO
	}
		
	return J;
}

uint32_t TMNTGBA2Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | StandardDPad(pad);
	bool Jump=0, Attack=0, SpinKick=0, SpecialMove=0, Pause=0, Select=0;

	const LegacyWpad * wp = LegacyWpad_Data(pad);
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		Jump = (wp->btns_h & LW_BUTTON_A);
		Attack = (fabs(wp->gforce.x)> 1.5) || (wp->btns_h & LW_BUTTON_B);
		SpinKick = (fabs(wp->exp.nunchuk.gforce.x)> 0.5);
		SpecialMove = (wp->btns_h & LW_NUNCHUK_BUTTON_Z);
		Pause = (wp->btns_h & LW_BUTTON_PLUS);
		Select = (wp->btns_h & LW_BUTTON_MINUS);
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		Jump = (wp->btns_h & LW_CLASSIC_BUTTON_B);
		Attack = (wp->btns_h & LW_CLASSIC_BUTTON_A);
		SpinKick = (wp->btns_h & LW_CLASSIC_BUTTON_X);
		Pause = (wp->btns_h & LW_CLASSIC_BUTTON_PLUS);
		Select = (wp->btns_h & LW_CLASSIC_BUTTON_MINUS);
		SpecialMove = (wp->btns_h & (LW_CLASSIC_BUTTON_FULL_L | LW_CLASSIC_BUTTON_FULL_R | LW_CLASSIC_BUTTON_ZR));
	} else {
		Jump = (wp->btns_h & LW_BUTTON_A);
		Attack = (fabs(wp->gforce.x)> 1.5) || (wp->btns_h & LW_BUTTON_B);
		Pause = (wp->btns_h & LW_BUTTON_PLUS);
		Select = (wp->btns_h & LW_BUTTON_MINUS);
		SpinKick = (wp->btns_h & LW_BUTTON_1);
		SpecialMove = (wp->btns_h & LW_BUTTON_2);
	}

	uint32_t gc = LegacyPad_ButtonsHeld(pad);
	// DPad moves
	if (gc & LP_BUTTON_UP)
		J |= VBA_UP;
	if (gc & LP_BUTTON_DOWN)
		J |= VBA_DOWN;
	if (gc & LP_BUTTON_LEFT)
		J |= VBA_LEFT;
	if (gc & LP_BUTTON_RIGHT)
		J |= VBA_RIGHT;
	// Jump
	if (gc & LP_BUTTON_A) J |= VBA_BUTTON_A;
	// Attack
	if (gc & LP_BUTTON_X) Attack = true;
	// Spin kick
	if (gc & LP_BUTTON_Y) SpinKick = true;
	// Pause
	if (gc & LP_BUTTON_START) Pause = true;
	// Select
	if (gc & LP_TRIGGER_Z) Select = true;
	// SpecialMove
	if (gc & LP_TRIGGER_L || gc & LP_TRIGGER_R) SpecialMove = true;

	if (Jump) J |= VBA_BUTTON_A;
	if (Attack) J |= VBA_BUTTON_B;
	if (SpinKick) J |= VBA_BUTTON_R;
	if (Pause) J |= VBA_BUTTON_START;
	if (Select) J |= VBA_BUTTON_SELECT;
	if (SpecialMove) {
		J |= VBA_UP | VBA_BUTTON_A;
	}
		
	return J;
}

uint32_t HarryPotter1GBCInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | StandardDPad(pad) | DecodeGamecube(pad);

	const LegacyWpad * wp = LegacyWpad_Data(pad);

	// Pause Menu
	if (wp->btns_h & LW_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map (well, it tells you where you are)
	if (wp->btns_h & LW_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// talk or interact
	if (wp->btns_h & LW_BUTTON_A)
		J |= VBA_BUTTON_A;
	// cancel
	if (wp->btns_h & LW_BUTTON_B)
		J |= VBA_BUTTON_B;
	// spells
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_A;

	if (wp->btns_h & LW_BUTTON_1)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & LW_BUTTON_1)
		J |= VBA_BUTTON_R;

	// Run (uses emulator speed button)
	if ((wp->exp.type == LW_EXP_NUNCHUK) && (wp->btns_h & LW_NUNCHUK_BUTTON_Z))
		J |= VBA_SPEED;
	// Camera, just tells you what room you are in. CAKTODO make press and release trigger it
	if ((wp->exp.type == LW_EXP_NUNCHUK) && (wp->btns_h & LW_NUNCHUK_BUTTON_C))
		J |= VBA_BUTTON_SELECT;

	// CAKTODO spell gestures

	return J;
}

uint32_t HarryPotter2GBCInput(unsigned short pad) {
	return HarryPotter1GBCInput(pad);
}

uint32_t HarryPotter1Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) 
			| DecodeGamecube(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);

	// DPad works in the map
	if (wp->btns_h & LW_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;
	if (wp->btns_h & LW_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & LW_BUTTON_UP)
		J |= VBA_UP;
	if (wp->btns_h & LW_BUTTON_DOWN)
		J |= VBA_DOWN;

	// Pause
	if (wp->btns_h & LW_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map
	if (wp->btns_h & LW_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// talk or interact
	if (wp->btns_h & LW_BUTTON_A)
		J |= VBA_BUTTON_A;
	// spells
	if (wp->btns_h & LW_BUTTON_B)
		J |= VBA_BUTTON_B;
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_B;

	if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
		J |= VBA_BUTTON_R;

	// Run (uses emulator speed button)
	if ((wp->exp.type == LW_EXP_NUNCHUK) && (wp->btns_h & LW_NUNCHUK_BUTTON_Z))
		J |= VBA_SPEED;
	// Flute
	if ((wp->exp.type == LW_EXP_NUNCHUK) && (wp->btns_h & LW_NUNCHUK_BUTTON_C))
		J |= VBA_BUTTON_L;

	// CAKTODO spell gestures

	return J;
}

uint32_t HarryPotter2Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) 
			| DecodeGamecube(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);

	// DPad works in the map
	if (wp->btns_h & LW_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;
	if (wp->btns_h & LW_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & LW_BUTTON_UP)
		J |= VBA_UP;
	if (wp->btns_h & LW_BUTTON_DOWN)
		J |= VBA_DOWN;

	// Pause
	if (wp->btns_h & LW_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map
	if (wp->btns_h & LW_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// talk or interact or sneak
	if (wp->btns_h & LW_BUTTON_A)
		J |= VBA_BUTTON_B;
	// spells
	if (wp->btns_h & LW_BUTTON_B)
		J |= VBA_BUTTON_A;
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_A;

	if (wp->btns_h & LW_BUTTON_1)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & LW_BUTTON_2)
		J |= VBA_BUTTON_L;

	// Sneak instead of run
	if ((wp->exp.type == LW_EXP_NUNCHUK) && (wp->btns_h & LW_NUNCHUK_BUTTON_Z))
		J |= VBA_BUTTON_B;
	// Jump with C button
	if ((wp->exp.type == LW_EXP_NUNCHUK) && (wp->btns_h & LW_NUNCHUK_BUTTON_C))
		J |= VBA_BUTTON_R;

	// CAKTODO spell gestures

	return J;
}

uint32_t HarryPotter3Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) 
			| DecodeGamecube(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);

	// DPad works in the map
	if (wp->btns_h & LW_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;
	if (wp->btns_h & LW_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & LW_BUTTON_UP)
		J |= VBA_UP;
	if (wp->btns_h & LW_BUTTON_DOWN)
		J |= VBA_DOWN;

	// Pause
	if (wp->btns_h & LW_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map doesn't exist. Options instead
	if (wp->btns_h & LW_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// talk or interact
	if (wp->btns_h & LW_BUTTON_A)
		J |= VBA_BUTTON_A;
	// spells
	if (wp->btns_h & LW_BUTTON_B)
		J |= VBA_BUTTON_B;
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_B;

	// Change spells
	if (wp->btns_h & LW_BUTTON_1)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & LW_BUTTON_2)
		J |= VBA_BUTTON_R;

	// Run (uses emulator speed button)
	if ((wp->exp.type == LW_EXP_NUNCHUK) && (wp->btns_h & LW_NUNCHUK_BUTTON_Z))
		J |= VBA_SPEED;

	// CAKTODO spell gestures
	// swing sideways for Flipendo
	// point at ceiling for Lumos

	return J;
}

uint32_t HarryPotter4Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);

	const LegacyWpad * wp = LegacyWpad_Data(pad);

	// DPad works in the map
	if (wp->btns_h & LW_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;
	if (wp->btns_h & LW_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & LW_BUTTON_UP)
		J |= VBA_UP;
	if (wp->btns_h & LW_BUTTON_DOWN)
		J |= VBA_DOWN;

	// Pause
	if (wp->btns_h & LW_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map doesn't exist. Select Needed for starting game.
	if (wp->btns_h & LW_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// talk or interact or jinx
	if (wp->btns_h & LW_BUTTON_A)
		J |= VBA_BUTTON_A;
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_A;
	// Charms
	if (wp->btns_h & LW_BUTTON_B)
		J |= VBA_BUTTON_B;

	// L and R
	if (wp->btns_h & LW_BUTTON_1)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & LW_BUTTON_2)
		J |= VBA_BUTTON_R;

	// Run (uses emulator speed button)
	if ((wp->exp.type == LW_EXP_NUNCHUK) && (wp->btns_h & LW_NUNCHUK_BUTTON_Z))
		J |= VBA_SPEED;

	// CAKTODO spell gestures
	// swing sideways for Flipendo
	// point at ceiling for Lumos

	return J;
}

bool GetPointerInGame(unsigned short pad, float* u, float* v)
{
	if (!u || !v || !controller[pad]) return false;
	const InputPadData& data = controller[pad]->getPadData();
	if (!data.validPointer) return false;

	EmulatorVideoDriver* emuVideo = platform->getVideo()->getEmulatorVideo();
	if (emuVideo && emuVideo->mapPointerToUnit(data.cursor_x, data.cursor_y, data.isTouch, u, v))
		return true;

	// assume the picture fills the whole canvas
	float fx = data.cursor_x / (float)platform->getVideo()->getScreenWidth();
	float fy = data.cursor_y / (float)platform->getVideo()->getScreenHeight();
	*u = fx < 0.0f ? 0.0f : (fx > 1.0f ? 1.0f : fx);
	*v = fy < 0.0f ? 0.0f : (fy > 1.0f ? 1.0f : fy);
	return true;
}

uint32_t HarryPotter5Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);

	const LegacyWpad * wp = LegacyWpad_Data(pad);

	// Wand cursor, normally controlled by DPad, now controlled by Wiimote!
	int cx = 0;
	int cy = 0;
	uint8_t WandOut = CPUReadByte(0x200e0dd);
	if (WandOut && CursorValid) {
		cx = (CursorX * 268)/640;
		cy = (CursorY * 187)/480;
		if (cx<0x14) cx=0x14;
		else if (cx>0xf8) cx=0xf8;
		if (cy<0x13) cy=0x13;
		else if (cy>0xa8) cy=0xa8;
		CPUWriteByte(0x200e0fe, cx);
		CPUWriteByte(0x200e102, cy);
	}

	// DPad works in the map
	if (wp->btns_h & LW_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;
	if (wp->btns_h & LW_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & LW_BUTTON_UP)
		J |= VBA_UP;
	if (wp->btns_h & LW_BUTTON_DOWN)
		J |= VBA_DOWN;

	// Pause
	if (wp->btns_h & LW_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map (actually objectives)
	if (wp->btns_h & LW_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// talk or interact
	if (wp->btns_h & LW_BUTTON_A)
		J |= VBA_BUTTON_A;
	// spells
	if (wp->btns_h & LW_BUTTON_B)
		J |= VBA_BUTTON_B;

	// Run (uses emulator speed button)
	if ((wp->exp.type == LW_EXP_NUNCHUK) && (wp->btns_h & LW_NUNCHUK_BUTTON_Z))
		J |= VBA_SPEED;

	// CAKTODO spell gestures

	return J;
}

// WarioWare Twisted
uint32_t TwistedInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);

	if (wp->exp.type == LW_EXP_NUNCHUK) {
		TiltSideways = false;
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select and L do nothing!
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & LW_BUTTON_1)
			J |= VBA_BUTTON_L | VBA_SPEED;
		if (wp->btns_h & LW_BUTTON_2)
			J |= VBA_BUTTON_L | VBA_SPEED;

		// A Button
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// B Button
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Grab an icon and prevent menu from spinning
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z)
			J |= VBA_BUTTON_R;
		// Calibrate
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C) {
			CalibrateWario = true;
		} else CalibrateWario = false;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		TiltSideways = false;
		J |= StandardDPad(pad) | StandardClassic(pad);

	} else {
		TiltSideways = true;
		J |= StandardSideways(pad);
		if (wp->btns_h & LW_BUTTON_B) {
			CalibrateWario = true;
		} else CalibrateWario = false;
	}
	return J;
}

uint32_t KirbyTntInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		TiltSideways = false;
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select and L do nothing!
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & LW_BUTTON_1)
			J |= VBA_BUTTON_B;
		if (wp->btns_h & LW_BUTTON_2)
			J |= VBA_BUTTON_A;

		// A Button
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// B Button
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Speed
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C)
			J |= VBA_SPEED;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		TiltSideways = false;
		J |= StandardDPad(pad) | DecodeClassic(pad);
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
	} else {
		TiltSideways = true;
		J |= StandardSideways(pad);
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_SPEED;
	}
	return J;
}

uint32_t MohInfiltratorInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	if (wp->exp.type == LW_EXP_NONE)
		J |= DecodeWiimote(pad);
	else if (wp->exp.type == LW_EXP_CLASSIC) {
		J |= DecodeClassic(pad);
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		// Pause, objectives, menu
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Action button, use
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_L;
		// Use sights/scope, not needed in this game
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A | VBA_BUTTON_L;
		// Shoot
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_BUTTON_A;
		// Reload
		if (fabs(wp->gforce.y)> 1.6 || wp->btns_h & LW_BUTTON_UP || wp->btns_h & LW_BUTTON_2)
			J |= VBA_BUTTON_L;
		// Strafe
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C)
			J |= VBA_BUTTON_R;
		// Change weapon
		if (wp->btns_h & LW_BUTTON_LEFT || wp->btns_h & LW_BUTTON_RIGHT)
			J |= VBA_BUTTON_B;
		// Speed
		if (wp->btns_h & LW_BUTTON_1)
			J |= VBA_SPEED;
	} else
		J |= DecodeWiimote(pad);
	return J;
}

uint32_t MohUndergroundInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeClassic(pad) | DecodeWiimote(pad) | DecodeGamecube(pad);
	static bool crouched = false;
	const LegacyWpad * wp = LegacyWpad_Data(pad);

	if (wp->exp.type == LW_EXP_NUNCHUK) {
		// Joystick controls L and R, not left and right
		if (J & VBA_LEFT)
		J |= VBA_BUTTON_L;
		if (J & VBA_RIGHT)
		J |= VBA_BUTTON_R;
		J &= ~(VBA_LEFT | VBA_RIGHT);
		// Cursor controls left and right for turning
		CursorVisible = true;
		if (CursorValid) {
			if (CursorX < 320 - 40)
			J |= VBA_LEFT;
			else if (CursorX> 320 + 40)
			J |= VBA_RIGHT;
		}
		// Crouch
		if (wp->btns_h & LW_BUTTON_DOWN)
		crouched = !crouched;

		// Pause, objectives, menu
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Action button, not needed in this game
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= 0;
		// Use sights/scope, not needed in this game
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Shoot
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_BUTTON_A;
		// Reload
		if (fabs(wp->gforce.y)> 1.6 || wp->btns_h & LW_BUTTON_UP || wp->btns_h & LW_BUTTON_2)
			J |= VBA_BUTTON_SELECT;
		// Change weapon
		if (wp->btns_h & LW_BUTTON_LEFT || wp->btns_h & LW_BUTTON_RIGHT)
			J |= VBA_BUTTON_B;
		// Speed
		if (wp->btns_h & LW_BUTTON_1)
			J |= VBA_SPEED;
	} else {
		CursorVisible = false;
	}

	if (crouched && (!(J & VBA_BUTTON_L)) && (!(J & VBA_BUTTON_R)))
		J |= VBA_BUTTON_L | VBA_BUTTON_R;

	return J;
}

uint32_t BoktaiInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | StandardDPad(pad)
			| DecodeGamecube(pad) | DecodeClassic(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	static bool GunRaised = false;
	// Action
	if (wp->btns_h & LW_BUTTON_A)
		J |= VBA_BUTTON_A;
	// Hold Gun Del Sol up to the light to recharge
	if ((-wp->orient.pitch)> 45) {
		GunRaised = true;
	} else if ((-wp->orient.pitch) < 40) {
		GunRaised = false;
	}
	if (GunRaised)
		J |= VBA_BUTTON_A;
	// Fire Gun Del Sol
	if (wp->btns_h & LW_BUTTON_B)
		J |= VBA_BUTTON_B;
	// Look around or change subscreen
	if (wp->btns_h & LW_BUTTON_2)
		J |= VBA_BUTTON_R;
	// Start
	if (wp->btns_h & LW_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Select
	if (wp->btns_h & LW_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	if (wp->exp.type == LW_EXP_NUNCHUK) {
		// Look around
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C)
			J |= VBA_BUTTON_R;
		// Change element or change subscreen
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z)
			J |= VBA_BUTTON_L;

		if (wp->btns_h & LW_BUTTON_1)
			J |= VBA_SPEED;
	} else {
		// Change element or change subscreen
		if (wp->btns_h & LW_BUTTON_1)
			J |= VBA_BUTTON_L;
	}
	return J;
}

uint32_t Boktai2Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | StandardDPad(pad)
			| DecodeGamecube(pad) | DecodeClassic(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	static bool GunRaised = false;
		// Action
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Hold gun or hand up to the light to recharge
		if ((-wp->orient.pitch)> 45) {
			GunRaised = true;
		} else if ((-wp->orient.pitch) < 40) {
			GunRaised = false;
		}
		if (GunRaised)
			J |= VBA_BUTTON_A;
		// Fire Gun Del Sol
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Swing sword or hammer or stab spear
		if (fabs(wp->gforce.x)> 1.8)
			J |= VBA_BUTTON_B;
		// Look around or change subscreen
		if (wp->btns_h & LW_BUTTON_2)
			J |= VBA_BUTTON_R;
		// Start
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;

	if (wp->exp.type == LW_EXP_NUNCHUK) {
		// Look around
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C)
			J |= VBA_BUTTON_R;
		// Change element or change subscreen
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z)
			J |= VBA_BUTTON_L;

		if (wp->btns_h & LW_BUTTON_1)
		J |= VBA_SPEED;
	} else {
		// Change element or change subscreen
		if (wp->btns_h & LW_BUTTON_1)
		J |= VBA_BUTTON_L;
	}

	return J;
}

uint32_t OnePieceInput(unsigned short pad) {
	// Only Nunchuk and Gamecube controls available
	// Wiimote and Classic controls depend on user configuration
	uint32_t J = StandardMovement(pad) 
		    | DecodeWiimote(pad) | DecodeClassic(pad);
	static uint32_t LastDir = VBA_RIGHT;
	bool JumpButton=0, AttackButton=0, ViewButton=0, CharacterButton=0, PauseButton=0,
	DashButton=0, GrabButton=0, SpeedButton=0, AttackUpButton = 0;
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	// Nunchuk controls are based on One Piece: Unlimited Adventure for the Wii
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		J |= StandardDPad(pad);
		JumpButton = wp->btns_h & LW_BUTTON_B;
		AttackButton = wp->btns_h & LW_BUTTON_A;
		CharacterButton = wp->btns_h & LW_BUTTON_MINUS;
		PauseButton = wp->btns_h & LW_BUTTON_PLUS;
		DashButton = wp->btns_h & LW_NUNCHUK_BUTTON_C;
		GrabButton = wp->btns_h & LW_NUNCHUK_BUTTON_Z;
		ViewButton = wp->btns_h & LW_BUTTON_1; // doesn't do anything?
		SpeedButton = wp->btns_h & LW_BUTTON_2;
	}
	// Gamecube controls are based on One Piece Grand Adventure
	{
		uint32_t gc = LegacyPad_ButtonsHeld(pad);
		signed char gc_px = LegacyPad_SubStickX(pad);
		if (gc_px > 70) J |= VBA_SPEED;
		JumpButton = JumpButton || gc & LP_BUTTON_Y;
		AttackButton = AttackButton || gc & LP_BUTTON_A;
		GrabButton = GrabButton || gc & LP_BUTTON_B;
		AttackUpButton = AttackUpButton || gc & LP_BUTTON_X;
		DashButton = DashButton || gc & LP_TRIGGER_L;
		PauseButton = PauseButton || gc & LP_BUTTON_START;
		CharacterButton = CharacterButton || gc & LP_TRIGGER_R; // supposed to be block
	}
	
	if (JumpButton) J |= VBA_BUTTON_A;
	if (AttackButton) J |= VBA_BUTTON_B;
	if (AttackUpButton) J |= VBA_UP | VBA_BUTTON_B;
	if (CharacterButton) J |= VBA_BUTTON_L;
	if (DashButton) J |= LastDir;
	if (PauseButton) J |= VBA_BUTTON_START;
	if (GrabButton) J |= VBA_BUTTON_R;
	if (SpeedButton) J |= VBA_SPEED;
	if (ViewButton) J |= VBA_BUTTON_SELECT; // doesn't do anything?

	if (J & VBA_RIGHT) LastDir = VBA_RIGHT;
	else if (J & VBA_LEFT) LastDir = VBA_LEFT;
	
	return J;
}

uint32_t HobbitInput(unsigned short pad) {
	// Only Nunchuk controls available
	// Wiimote, Gamecube and Classic controls depend on user configuration
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad) | 
	DecodeWiimote(pad) | DecodeClassic(pad);
	bool AbilityButton=0, AttackButton=0, UseButton=0, ChangeSkillButton=0, PauseButton=0,
	ItemsButton=0, SpeedButton=0;
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	// Nunchuk controls are made up
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		J |= StandardDPad(pad);
		AbilityButton = wp->btns_h & LW_BUTTON_B;
		UseButton = wp->btns_h & LW_BUTTON_A;
		PauseButton = wp->btns_h & LW_BUTTON_PLUS;
		ItemsButton = wp->btns_h & LW_BUTTON_MINUS;
		SpeedButton = wp->btns_h & LW_NUNCHUK_BUTTON_C;
		ChangeSkillButton = wp->btns_h & LW_NUNCHUK_BUTTON_Z;
		AttackButton = (fabs(wp->gforce.x)> 1.5);
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
	// Only Nunchuk controls available
	// Wiimote, Gamecube and Classic controls depend on user configuration
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad) | 
	DecodeWiimote(pad) | DecodeClassic(pad);
	bool CancelButton=0, UseButton=0, ChangeCharButton=0, PauseButton=0,
	ItemsButton=0, SpeedButton=0, SelectButton=0;
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	// Nunchuk controls are made up
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		J |= StandardDPad(pad);
		CancelButton = wp->btns_h & LW_BUTTON_B;
		UseButton = wp->btns_h & LW_BUTTON_A;
		PauseButton = wp->btns_h & LW_BUTTON_PLUS;
		ItemsButton = wp->btns_h & LW_BUTTON_MINUS;
		SpeedButton = wp->btns_h & LW_NUNCHUK_BUTTON_C;
		ChangeCharButton = wp->btns_h & LW_NUNCHUK_BUTTON_Z;
		CancelButton = CancelButton || (fabs(wp->gforce.x)> 1.5);
		SelectButton = wp->btns_h & LW_BUTTON_1;
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
	// Only Nunchuk controls available
	// Wiimote, Gamecube and Classic controls depend on user configuration
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad) | 
	DecodeWiimote(pad) | DecodeClassic(pad);
	bool AbilityButton=0, AttackButton=0, UseButton=0, ChangeSkillButton=0, PauseButton=0,
	ItemsButton=0, SpeedButton=0;
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	// Nunchuk controls are made up
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		J |= StandardDPad(pad);
		AbilityButton = wp->btns_h & LW_BUTTON_B;
		UseButton = wp->btns_h & LW_BUTTON_A;
		PauseButton = wp->btns_h & LW_BUTTON_PLUS;
		ItemsButton = wp->btns_h & LW_BUTTON_MINUS;
		SpeedButton = wp->btns_h & LW_NUNCHUK_BUTTON_C;
		ChangeSkillButton = wp->btns_h & LW_NUNCHUK_BUTTON_Z;
		AttackButton = (fabs(wp->gforce.x)> 1.5);
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
	// Only Nunchuk and Classic controls available
	// Wiimote and Gamecube controls depend on user configuration
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeWiimote(pad) | DecodeClassic(pad);
	bool JumpButton=0, AttackButton=0, GuardButton=0, PauseButton=0, SelectButton=0, SpeedButton=0;
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	// Nunchuk controls are based on Castlevania Wii
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		AttackButton = (fabs(wp->gforce.x)> 1.5) || (fabs(wp->gforce.y)> 1.5) || wp->btns_h & LW_BUTTON_B;
		JumpButton = wp->btns_h & LW_NUNCHUK_BUTTON_C;
		GuardButton = wp->btns_h & LW_NUNCHUK_BUTTON_Z;
		PauseButton = wp->btns_h & LW_BUTTON_PLUS;
		SelectButton = wp->btns_h & LW_BUTTON_MINUS;
		SpeedButton = (wp->btns_h & LW_BUTTON_A) && (wp->btns_h & LW_BUTTON_B);
	// Classic controls are based on ...?
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		AttackButton = wp->btns_h & LW_CLASSIC_BUTTON_B;
		JumpButton = wp->btns_h & (LW_CLASSIC_BUTTON_FULL_R | LW_CLASSIC_BUTTON_ZR);
		GuardButton = wp->btns_h & (LW_CLASSIC_BUTTON_FULL_L | LW_CLASSIC_BUTTON_ZL);
		PauseButton = wp->btns_h & LW_CLASSIC_BUTTON_PLUS;
		SelectButton = wp->btns_h & LW_CLASSIC_BUTTON_MINUS;
		SpeedButton = wp->btns_h & LW_CLASSIC_BUTTON_A;
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
	// Only Nunchuk and Classic controls available
	// Wiimote and Gamecube controls depend on user configuration
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeWiimote(pad) | DecodeClassic(pad);
	bool JumpButton=0, AttackButton=0, ShootButton=0, GuardButton=0, PauseButton=0, SelectButton=0, SpeedButton=0;
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	// Nunchuk controls are based on Castlevania Wii
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		ShootButton = wp->btns_h & LW_BUTTON_A;
		AttackButton = (fabs(wp->gforce.x)> 1.5) || (fabs(wp->gforce.y)> 1.5) || wp->btns_h & LW_BUTTON_B;
		JumpButton = wp->btns_h & LW_NUNCHUK_BUTTON_C;
		GuardButton = wp->btns_h & LW_NUNCHUK_BUTTON_Z;
		PauseButton = wp->btns_h & LW_BUTTON_PLUS;
		SelectButton = wp->btns_h & LW_BUTTON_MINUS;
		SpeedButton = (wp->btns_h & LW_BUTTON_A) && (wp->btns_h & LW_BUTTON_B);
	// Classic controls are based on ...?
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		ShootButton = wp->btns_h & LW_CLASSIC_BUTTON_Y;
		AttackButton = wp->btns_h & LW_CLASSIC_BUTTON_B;
		JumpButton = wp->btns_h & (LW_CLASSIC_BUTTON_FULL_R | LW_CLASSIC_BUTTON_ZR);
		GuardButton = wp->btns_h & (LW_CLASSIC_BUTTON_FULL_L | LW_CLASSIC_BUTTON_ZL);
		PauseButton = wp->btns_h & LW_CLASSIC_BUTTON_PLUS;
		SelectButton = wp->btns_h & LW_CLASSIC_BUTTON_MINUS;
		SpeedButton = wp->btns_h & LW_CLASSIC_BUTTON_A;
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
	// Only Nunchuk and Classic controls available
	// Wiimote and Gamecube controls depend on user configuration
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeWiimote(pad) | DecodeClassic(pad);
	bool JumpButton=0, AttackButton=0, ShootButton=0, GuardButton=0, PauseButton=0, SelectButton=0, SpeedButton=0, HyperButton=0;
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	// Nunchuk controls are based on Castlevania Wii
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		ShootButton = wp->btns_h & LW_BUTTON_A;
		AttackButton = (fabs(wp->gforce.x)> 1.5) || (fabs(wp->gforce.y)> 1.5) || wp->btns_h & LW_BUTTON_B;
		JumpButton = wp->btns_h & LW_NUNCHUK_BUTTON_C;
		GuardButton = wp->btns_h & LW_NUNCHUK_BUTTON_Z;
		PauseButton = wp->btns_h & LW_BUTTON_PLUS;
		SelectButton = wp->btns_h & LW_BUTTON_MINUS;
		SpeedButton = (wp->btns_h & LW_BUTTON_A) && (wp->btns_h & LW_BUTTON_B);
		HyperButton = wp->btns_h & LW_BUTTON_DOWN;
	// Classic controls are based on ...?
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		ShootButton = wp->btns_h & LW_CLASSIC_BUTTON_Y;
		AttackButton = wp->btns_h & LW_CLASSIC_BUTTON_B;
		JumpButton = wp->btns_h & (LW_CLASSIC_BUTTON_FULL_R | LW_CLASSIC_BUTTON_ZR);
		GuardButton = wp->btns_h & (LW_CLASSIC_BUTTON_FULL_L | LW_CLASSIC_BUTTON_ZL);
		PauseButton = wp->btns_h & LW_CLASSIC_BUTTON_PLUS;
		SelectButton = wp->btns_h & LW_CLASSIC_BUTTON_MINUS;
		SpeedButton = wp->btns_h & LW_CLASSIC_BUTTON_A;
		HyperButton = wp->btns_h & LW_CLASSIC_BUTTON_X;
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
	// Only Nunchuk and Classic controls available
	// Wiimote and Gamecube controls depend on user configuration
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeWiimote(pad) | DecodeClassic(pad);
	bool JumpButton=0, AttackButton=0, ShootButton=0, GuardButton=0, PauseButton=0, SelectButton=0, SpeedButton=0, HyperButton=0,
	LButton=0, RButton=0;
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	// Nunchuk controls are based on Castlevania Wii
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		ShootButton = wp->btns_h & LW_BUTTON_A;
		AttackButton = (fabs(wp->gforce.x)> 1.5) || (fabs(wp->gforce.y)> 1.5) || wp->btns_h & LW_BUTTON_B;
		JumpButton = wp->btns_h & LW_NUNCHUK_BUTTON_C;
		GuardButton = wp->btns_h & LW_NUNCHUK_BUTTON_Z;
		PauseButton = wp->btns_h & LW_BUTTON_PLUS;
		SelectButton = wp->btns_h & LW_BUTTON_MINUS;
		SpeedButton = (wp->btns_h & LW_BUTTON_A) && (wp->btns_h & LW_BUTTON_B);
		HyperButton = wp->btns_h & LW_BUTTON_DOWN;
		LButton = wp->btns_h & LW_BUTTON_1;
		RButton = wp->btns_h & LW_BUTTON_2;
	// Classic controls are based on ...?
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		ShootButton = wp->btns_h & LW_CLASSIC_BUTTON_Y;
		AttackButton = wp->btns_h & LW_CLASSIC_BUTTON_B;
		JumpButton = wp->btns_h & (LW_CLASSIC_BUTTON_FULL_R);
		GuardButton = wp->btns_h & (LW_CLASSIC_BUTTON_FULL_L);
		PauseButton = wp->btns_h & LW_CLASSIC_BUTTON_PLUS;
		SelectButton = wp->btns_h & LW_CLASSIC_BUTTON_MINUS;
		SpeedButton = wp->btns_h & LW_CLASSIC_BUTTON_A;
		HyperButton = wp->btns_h & LW_CLASSIC_BUTTON_X;
		LButton = wp->btns_h & LW_CLASSIC_BUTTON_ZL;
		RButton = wp->btns_h & LW_CLASSIC_BUTTON_ZR;
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
	// Only Nunchuk and Classic controls available
	// Wiimote and Gamecube controls depend on user configuration
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeWiimote(pad) | DecodeClassic(pad);
	bool JumpButton=0, ShootButton=0, PauseButton=0, SelectButton=0, SpeedButton=0, NorButton=0, BatButton=0;
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		JumpButton = wp->btns_h & LW_BUTTON_A;
		ShootButton = wp->btns_h & LW_BUTTON_B;
		PauseButton = wp->btns_h & LW_BUTTON_PLUS;
		SelectButton = wp->btns_h & LW_BUTTON_MINUS;
		SpeedButton = wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2;
		NorButton = wp->btns_h & LW_NUNCHUK_BUTTON_Z;
		BatButton = wp->btns_h & LW_NUNCHUK_BUTTON_C;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		JumpButton = wp->btns_h & (LW_CLASSIC_BUTTON_B | LW_CLASSIC_BUTTON_A);
		ShootButton = wp->btns_h & LW_CLASSIC_BUTTON_Y;
		PauseButton = wp->btns_h & LW_CLASSIC_BUTTON_PLUS;
		SelectButton = wp->btns_h & LW_CLASSIC_BUTTON_MINUS;
		SpeedButton = wp->btns_h & (LW_CLASSIC_BUTTON_ZL | LW_CLASSIC_BUTTON_FULL_L);
		NorButton = wp->btns_h & LW_CLASSIC_BUTTON_X;
		BatButton = wp->btns_h & (LW_CLASSIC_BUTTON_ZR | LW_CLASSIC_BUTTON_FULL_R);
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

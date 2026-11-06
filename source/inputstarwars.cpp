/****************************************************************************
 * Visual Boy Advance GX
 *
 * Carl Kenner May 2009
 * Daryl Borth 2026 (Decoupled Input Architecture)
 *
 * inputstarwars.cpp
 *
 * Wii/Gamecube controls for Star Wars games
 ***************************************************************************/

#include <stdint.h>
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

#include "vba/gba/GBA.h"
#include "vba/gba/bios.h"
#include "vba/gba/GBAinline.h"
#include "legacyinput.h"

uint32_t LegoStarWars1Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	// Rumble when they lose health!
	uint8_t Health = 0;
	static uint8_t OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	const LegacyWpad * wp = LegacyWpad_Data(pad);

	// Start/Select
	if (wp->btns_h & LW_BUTTON_PLUS)
		J |= VBA_BUTTON_START;

	if (wp->exp.type == LW_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		// build, use force
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z)
			J |= VBA_BUTTON_L;
		// change characters
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Shoot
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 )
			J |= VBA_BUTTON_B;
		// Force power, special ability
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		J |= DecodeClassic(pad);
	} else J |= DecodeWiimote(pad);
	return J;
}

uint32_t LegoStarWars2Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	// Rumble when they lose health!
	uint8_t Health = 0;
	static uint8_t OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	const LegacyWpad * wp = LegacyWpad_Data(pad);

	// Start/Select
	if (wp->btns_h & LW_BUTTON_PLUS)
		J |= VBA_BUTTON_START;

	if (wp->exp.type == LW_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		// build, force transform, pull lever
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z)
			J |= VBA_BUTTON_R;
		// change characters
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C || wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_BUTTON_SELECT;
		// grapple
		if (fabs(wp->gforce.y)> 1.6 )
			J |= VBA_BUTTON_R;
		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Shoot
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 )
			J |= VBA_BUTTON_B;
		// Force power, vehicle special ability
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_L;
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;

	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		J |= DecodeClassic(pad);
	}
	return J;
}

uint32_t SWObiWanInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	// Rumble when they lose health!
	uint8_t Health = gbReadMemory(0xCFF2);
	static uint8_t OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	const LegacyWpad * wp = LegacyWpad_Data(pad);

	// Start/Select
	if (wp->btns_h & LW_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	if (wp->btns_h & LW_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	if (wp->exp.type == LW_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		// use the force
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z) {
			gbWriteMemory(0xCFF1, 2);
			J |= VBA_BUTTON_A;
		}
		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Shoot
		if (wp->btns_h & LW_BUTTON_B) {
			gbWriteMemory(0xCFF1, 0);
			J |= VBA_BUTTON_A;
		}
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			gbWriteMemory(0xCFF1, 1);
			J |= VBA_BUTTON_A;
		}
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		J |= DecodeClassic(pad);
	}
	return J;
}

uint32_t SWEpisode2Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	// Rumble when they lose health!
	uint8_t Health = CPUReadByte(0x3002fb3);
	static uint8_t OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

	const LegacyWpad * wp = LegacyWpad_Data(pad);

	// Start/Select
	if (wp->btns_h & LW_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	if (wp->btns_h & LW_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	if (wp->exp.type == LW_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Shoot (N/A)
		if (wp->btns_h & LW_BUTTON_B) {
			J |= VBA_BUTTON_A;
		}
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_A;
		}
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
		// CAKTODO
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_L;
		}
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z) {
			J |= VBA_BUTTON_R;
		}
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		J |= DecodeClassic(pad);
	}
	return J;
}

uint32_t SWEpisode3Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	// Rumble when they lose health!
	uint8_t Health = 0;//CPUReadByte(0x3002fb3);
	static uint8_t OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

	const LegacyWpad * wp = LegacyWpad_Data(pad);

	if (wp->exp.type == LW_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Shoot (N/A)
		if (wp->btns_h & LW_BUTTON_B) {
			J |= VBA_BUTTON_B;
		}
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_B;
		}
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
		// CAKTODO
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_L;
		}
		// Force
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z) {
			J |= VBA_BUTTON_R;
		}
		// Start/Select
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
	return J;
}

uint32_t SWJediPowerBattlesInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	// Rumble when they lose health!
	uint8_t Health = 0;//CPUReadByte(0x3002fb3);
	static uint8_t OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

	const LegacyWpad * wp = LegacyWpad_Data(pad);

	if (wp->exp.type == LW_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_A;
		}
		// Shoot ?
		if (wp->btns_h & LW_BUTTON_B) {
			J |= VBA_BUTTON_A;
		}
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
		// Block
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_R;
		}
		// Force
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z) {
			J |= VBA_BUTTON_L;
		}
		// Start/Select
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
	return J;
}

uint32_t SWTrilogyInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	// Rumble when they lose health!
	uint8_t Health = 0;//CPUReadByte(0x3002fb3);
	static uint8_t OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

	const LegacyWpad * wp = LegacyWpad_Data(pad);

	if (wp->exp.type == LW_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_B;
		}
		// Shoot
		if (wp->btns_h & LW_BUTTON_B) {
			J |= VBA_BUTTON_B;
		}
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
		// Block
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_L;
		}
		// Force
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z) {
			J |= VBA_BUTTON_R;
		}
		// Start/Select
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
	return J;
}

uint32_t SWEpisode4Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	// Rumble when they lose health!
	uint8_t Health = 0;
	static uint8_t OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

	const LegacyWpad * wp = LegacyWpad_Data(pad);

	if (wp->exp.type == LW_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_B;
		}
		// Shoot / run
		if (wp->btns_h & LW_BUTTON_B) {
			J |= VBA_BUTTON_B;
		}
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
		// Block
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_B;
		}
		// Force
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z) {

		}
		// Start/Select
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
	return J;
}

uint32_t SWEpisode5Input(unsigned short pad) {
	return SWEpisode4Input(pad);
}

uint32_t SWEpisode6Input(unsigned short pad) {
	return SWEpisode4Input(pad);
}

uint32_t SWYodaStoriesInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	// Rumble when they lose health!
	uint8_t Health = 0;//gbReadMemory(0xD58A); // actually health bar progress
	static uint8_t OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

	const LegacyWpad * wp = LegacyWpad_Data(pad);

	if (wp->exp.type == LW_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		// Drag object, get out of dialog
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_B;

		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			// should change weapon here
			J |= VBA_BUTTON_A;
		}
		// Shoot / run
		if (wp->btns_h & LW_BUTTON_B) {
			// should change weapon here
			J |= VBA_BUTTON_A;
		}
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
		// Force
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z) {
			// should change weapon here
			J |= VBA_BUTTON_A;
		}
		// Start/Select
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
	return J;
}

uint32_t SWNDAInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	// Rumble when they lose health!
	uint8_t Health = 0;
	static uint8_t OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

	const LegacyWpad * wp = LegacyWpad_Data(pad);

	if (wp->exp.type == LW_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		// Jump attack, the only kind of jumping in this game
		// Note, this only works if you press B first, then A
		if (wp->btns_h & LW_BUTTON_A) {
			J &= ~(VBA_DOWN | VBA_LEFT | VBA_RIGHT);
			J |= VBA_BUTTON_A | VBA_UP;
		}
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_A;
		}
		// Activate light saber
		if (wp->btns_h & LW_BUTTON_B) {
			J |= VBA_BUTTON_A;
		}
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
		// Force
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z) {
			J |= VBA_BUTTON_R;
		}
		// Block
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_B;
		}
		// Change force power
		if (wp->btns_h & (LW_BUTTON_LEFT | LW_BUTTON_RIGHT | LW_BUTTON_UP | LW_BUTTON_DOWN)) {
			J |= VBA_BUTTON_L;
		}
		// Start/Select
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
	return J;
}

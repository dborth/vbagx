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
#include "libgui/GuiInputController.h"

#include "vba/gba/GBA.h"
#include "vba/gba/bios.h"
#include "vba/gba/GBAinline.h"

uint32_t LegoStarWars1Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	uint8_t Health = 0;
	static uint8_t OldHealth = 0;
	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) J |= VBA_BUTTON_L; // Build/Force
	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A; // Jump
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) J |= VBA_BUTTON_B; // Shoot/Saber

	// Light saber (Shake X-axis)
	if (fabs(data.gforceX) > 1.5) J |= VBA_BUTTON_B;

	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_SPEED; // Speed/Grapple

	return J;
}

uint32_t LegoStarWars2Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	uint8_t Health = 0;
	static uint8_t OldHealth = 0;
	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_BUTTON_R; // Force/Grapple

	// Grapple (Shake Y-axis)
	if (fabs(data.gforceY) > 1.6) J |= VBA_BUTTON_R;

	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) J |= VBA_BUTTON_L;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A; // Jump
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) J |= VBA_BUTTON_B; // Shoot/Saber

	// Light saber (Shake X-axis)
	if (fabs(data.gforceX) > 1.5) J |= VBA_BUTTON_B;

	return J;
}

uint32_t SWObiWanInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	uint8_t Health = gbReadMemory(0xCFF2);
	static uint8_t OldHealth = 0;
	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) {
		gbWriteMemory(0xCFF1, 2);
		J |= VBA_BUTTON_A;
	}

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_B; // Jump
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) {
		gbWriteMemory(0xCFF1, 0); // Saber/Shoot
		J |= VBA_BUTTON_A;
	}

	// Light saber (Shake X-axis)
	if (fabs(data.gforceX) > 1.5) {
		gbWriteMemory(0xCFF1, 1);
		J |= VBA_BUTTON_A;
	}

	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_SPEED;

	return J;
}

uint32_t SWEpisode2Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	uint8_t Health = CPUReadByte(0x3002fb3);
	static uint8_t OldHealth = 0;
	if (Health < OldHealth) systemGameRumble(6);
	OldHealth = Health;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_B; // Jump
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) J |= VBA_BUTTON_A; // Shoot/Saber

	// Light saber (Shake X-axis)
	if (fabs(data.gforceX) > 1.5) J |= VBA_BUTTON_A;

	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) J |= VBA_BUTTON_L;
	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_BUTTON_R;

	return J;
}

uint32_t SWEpisode3Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	uint8_t Health = 0;
	static uint8_t OldHealth = 0;
	if (Health < OldHealth) systemGameRumble(6);
	OldHealth = Health;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) J |= VBA_BUTTON_B;

	// Light saber (Shake X-axis)
	if (fabs(data.gforceX) > 1.5) J |= VBA_BUTTON_B;

	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) J |= VBA_BUTTON_L;
	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_BUTTON_R;

	return J;
}

uint32_t SWJediPowerBattlesInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	uint8_t Health = 0;
	static uint8_t OldHealth = 0;
	if (Health < OldHealth) systemGameRumble(6);
	OldHealth = Health;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_B; // Jump
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) J |= VBA_BUTTON_A; // Shoot/Saber

	// Light saber (Shake X-axis)
	if (fabs(data.gforceX) > 1.5) J |= VBA_BUTTON_A;

	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) J |= VBA_BUTTON_L;
	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_BUTTON_R;

	return J;
}

uint32_t SWTrilogyInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	uint8_t Health = 0;
	static uint8_t OldHealth = 0;
	if (Health < OldHealth) systemGameRumble(6);
	OldHealth = Health;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) J |= VBA_BUTTON_B;

	// Light saber (Shake X-axis)
	if (fabs(data.gforceX) > 1.5) J |= VBA_BUTTON_B;

	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) J |= VBA_BUTTON_L;
	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_BUTTON_R;

	return J;
}

uint32_t SWEpisode4Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	uint8_t Health = 0;
	static uint8_t OldHealth = 0;
	if (Health < OldHealth) systemGameRumble(6);
	OldHealth = Health;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) J |= VBA_BUTTON_B;

	// Light saber (Shake X-axis)
	if (fabs(data.gforceX) > 1.5) J |= VBA_BUTTON_B;

	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) J |= VBA_BUTTON_L;
	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_SPEED;

	return J;
}

uint32_t SWEpisode5Input(unsigned short pad) {
	return SWEpisode4Input(pad);
}

uint32_t SWEpisode6Input(unsigned short pad) {
	return SWEpisode4Input(pad);
}

uint32_t SWYodaStoriesInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	uint8_t Health = 0;
	static uint8_t OldHealth = 0;
	if (Health < OldHealth) systemGameRumble(6);
	OldHealth = Health;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_B; // Drag object
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) J |= VBA_BUTTON_A; // Saber/Shoot

	// Light saber (Shake X-axis)
	if (fabs(data.gforceX) > 1.5) J |= VBA_BUTTON_A;

	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) J |= VBA_BUTTON_L;
	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_SPEED;

	return J;
}

uint32_t SWNDAInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	uint8_t Health = 0;
	static uint8_t OldHealth = 0;
	if (Health < OldHealth) systemGameRumble(6);
	OldHealth = Health;

	// Start / Select
	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	// Jump attack, the only kind of jumping in this game
	// Replaces Wiimote A behavior
	if (data.buttons_h & GUI_BTN_A) {
		J &= ~(VBA_DOWN | VBA_LEFT | VBA_RIGHT);
		J |= VBA_BUTTON_A | VBA_UP;
	}

	// Light saber / Activate light saber
	// Replaces raw motion swing (gforce.x) and Wiimote B
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) {
		J |= VBA_BUTTON_A;
	}

	// Light saber (Shake X-axis)
	if (fabs(data.gforceX) > 1.5) {
		J |= VBA_BUTTON_A;
	}

	// Speed
	if (data.buttons_h & (GUI_BTN_1 | GUI_BTN_2)) {
		J |= VBA_SPEED;
	}

	// Block
	// Replaces Nunchuk C (which maps to GUI_TRIGGER_L)
	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) {
		J |= VBA_BUTTON_B;
	}

	// Force
	// Replaces Nunchuk Z (which maps to GUI_TRIGGER_ZL / generic ZR)
	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) {
		J |= VBA_BUTTON_R;
	}

	// Change force power
	// Uses the D-Pad (StandardMovement already handles analog sticks independently)
	if (data.buttons_h & (GUI_BTN_LEFT | GUI_BTN_RIGHT | GUI_BTN_UP | GUI_BTN_DOWN)) {
		J |= VBA_BUTTON_L;
	}

	return J;
}

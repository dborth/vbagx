/****************************************************************************
 * Visual Boy Advance GX
 *
 * Carl Kenner Febuary 2009
 * Daryl Borth 2026 (Decoupled Input Architecture)
 *
 * inputmario.cpp
 *
 * Wii/Gamecube controls for individual games
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ogcsys.h>
#include <unistd.h>

#include "vbagx.h"
#include "button_mapping.h"
#include "audio.h"
#include "video.h"
#include "input.h"
#include "gameinput.h"
#include "vbasupport.h"
#include "libgui/GuiInputController.h"

#include "vba/gba/GBA.h"
#include "vba/gba/bios.h"
#include "vba/gba/GBAinline.h"

uint32_t MarioKartInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);
	static uint32_t frame = 0;

	u8 Health = 0;
	static u8 OldHealth = 0;
	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	// Start/Select
	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	// Use item
	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) J |= VBA_BUTTON_L;
	// Accelerate
	if (data.buttons_h & (GUI_BTN_A | GUI_BTN_Y)) J |= VBA_BUTTON_A;
	// Brake
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X)) J |= VBA_BUTTON_B;
	// Jump/Power slide
	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_BUTTON_R;

	// Jump (Shake Y-axis)
	if (fabs(data.gforceY) > 1.5) J |= VBA_BUTTON_R;

	// Steering (Tilt / Pitch)
	float fraction;
	if (data.pitch > 12) {
		fraction = (data.pitch - 12) / 60.0f;
		if ((frame % 60) / 60.0f < fraction)
			J |= VBA_LEFT;
	} else if (data.pitch < -12) {
		fraction = -(data.pitch + 10) / 60.0f;
		if ((frame % 60) / 60.0f < fraction)
			J |= VBA_RIGHT;
	}

	frame++;
	return J;
}

uint32_t Mario1DXInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	// Pause & Select
	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	// Jump
	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;

	// Run, pick up, spin attack
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) J |= VBA_BUTTON_B;
	if (fabs(data.gforceX) > 1.5) J |= VBA_BUTTON_B; // Starspin shoots when using fireflower

	// Crouch
	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) {
		J |= VBA_DOWN;
		J &= ~VBA_UP;
	}

	// Speed/Camera
	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_SPEED;

	return J;
}

uint32_t Mario1ClassicInput(unsigned short pad) {
	return Mario1DXInput(pad); // Mappings safely resolve exactly the same now
}

uint32_t MarioLand1Input(unsigned short pad) {
	return Mario1DXInput(pad);
}

uint32_t MarioLand2Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) J |= VBA_BUTTON_B;

	// Spin attack (Shake X-axis)
	if (fabs(data.gforceX) > 1.4) J |= VBA_DOWN | VBA_BUTTON_A;

	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) {
		J |= VBA_DOWN;
		J &= ~VBA_UP;
	}

	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_SPEED;

	return J;
}

uint32_t Mario2Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) J |= VBA_BUTTON_B;

	// Shake X-axis to pick up/throw
	if (fabs(data.gforceX) > 1.5) J |= VBA_BUTTON_B;

	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) J |= VBA_DOWN;
	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_SPEED;

	return J;
}

uint32_t MarioWorldInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	u8 FallState = CPUReadByte(0x3003FA1); // 0B = jump, 24 = fall
	u8 RidingYoshi = CPUReadByte(0x3004302); // 00 = not riding, 01 = riding
	static bool NeedStomp = false;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) J |= VBA_BUTTON_B;

	// Spin Attack / Tongue (Button or Shake)
	if (data.buttons_h & GUI_TRIGGER_R) J |= VBA_BUTTON_R;
	if (fabs(data.gforceX) > 1.5) J |= VBA_BUTTON_R;

	// Camera
	if (data.buttons_h & GUI_TRIGGER_L) J |= VBA_BUTTON_L;

	// Crouch / Stomp
	if (data.buttons_h & GUI_TRIGGER_ZL) {
		J |= VBA_DOWN;
		J &= ~VBA_UP;
		if (FallState != 0 && !RidingYoshi) NeedStomp = true;
	}

	if (NeedStomp && FallState == 0 && !RidingYoshi) {
		J |= VBA_BUTTON_R; // spin attack only works when on ground
		NeedStomp = false;
	}

	return J;
}

uint32_t Mario3Input(unsigned short pad) {
	return Mario1DXInput(pad);
}

uint32_t YoshiIslandInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X)) J |= VBA_BUTTON_B;
	if (data.buttons_h & (GUI_BTN_Y | GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_BUTTON_R; // Throw

	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) {
		J |= VBA_DOWN;
		J &= ~VBA_UP;
	}

	return J;
}

uint32_t UniversalGravitationInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	TiltScreen = true;
	TiltSideways = false;
	uint32_t J = StandardMovement(pad);

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (data.buttons_h & (GUI_BTN_B | GUI_BTN_X | GUI_BTN_Y)) J |= VBA_BUTTON_B; // Tongue

	if (data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL)) {
		J |= VBA_DOWN; // Crouch/Stomp
		J &= ~VBA_UP;
	}

	if (data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR)) J |= VBA_SPEED;

	return J;
}

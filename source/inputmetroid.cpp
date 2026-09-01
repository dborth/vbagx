/****************************************************************************
 * Visual Boy Advance GX
 *
 * Carl Kenner Febuary 2009
 * Daryl Borth 2026 (Decoupled Input Architecture)
 *
 * inputmetroid.cpp
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

uint32_t MetroidZeroInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	u8 BallState = CPUReadByte(0x30015df); // 0 = stand, 1 = crouch, 2 = ball
	u16 Health = CPUReadByte(0x3001536);
	static u16 OldHealth = 0;

	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;
	static int AimCount = 0;
	static int MissileCount = 0;

	if (BallState == 2) J &= ~VBA_UP;
	if (BallState == 1) J &= ~VBA_DOWN;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	// Jump & Fire (with accelerometer support for ball jump)
	if ((data.buttons_h & GUI_BTN_A) && BallState != 2) J |= VBA_BUTTON_A;
	else if (BallState == 2 && fabs(data.gforceY) > 1.5f) J |= VBA_BUTTON_A;

	if (data.buttons_h & GUI_BTN_B) J |= VBA_BUTTON_B;

	// Aiming (Button based)
	if (data.buttons_h & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
	if (data.buttons_h & GUI_TRIGGER_R) J |= VBA_BUTTON_R;

	// Aiming (Pitch based)
	if (data.pitch < -45 && BallState != 2) {
		J |= VBA_UP;
		AimCount = 0;
	} else if (data.pitch < -22 && BallState != 2) {
		if (AimCount >= 0) AimCount = -1;
	} else if (data.pitch > 45 && BallState == 0) {
		if (AimCount < 10) AimCount = 10;
	} else if (data.pitch > 22 && BallState != 2) {
		if (AimCount <= 0 || AimCount >= 10) AimCount = 1;
	} else {
		AimCount = 0;
	}

	// Missiles
	if (data.buttons_h & (GUI_BTN_X | GUI_BTN_Y)) MissileCount = 1;

	// Morph Ball
	if (data.buttons_h & GUI_TRIGGER_ZL) {
		if (BallState == 2) Morph = -1;
		else if (BallState == 1) Morph = 2;
		else Morph = 1;
	}

	switch (AimCount) {
		case 1:
			J &= ~(VBA_UP | VBA_DOWN | VBA_BUTTON_L);
			J |= VBA_BUTTON_L;
			AimCount++;
			break;
		case 2:
			J &= ~(VBA_UP | VBA_DOWN | VBA_BUTTON_L);
			J |= VBA_BUTTON_L | VBA_DOWN;
			AimCount++;
			break;
		case 3:
			J |= VBA_BUTTON_L;
			break;
		case -1:
			J &= ~(VBA_UP | VBA_DOWN | VBA_BUTTON_L);
			J |= 0;
			AimCount--;
			break;
		case -2:
			J &= ~(VBA_UP | VBA_DOWN | VBA_BUTTON_L);
			J |= VBA_BUTTON_L;
			AimCount--;
			break;
		case -3:
			J |= VBA_BUTTON_L;
			break;
		case 10:
			J |= VBA_BUTTON_A;
			AimCount++;
			break;
		case 11:
			J |= VBA_DOWN;
			AimCount++;
			break;
		case 12:
			J |= VBA_DOWN;
			break;
	}

	switch (MissileCount) {
		case 1: case 2: J |= VBA_BUTTON_R; MissileCount++; break;
		case 3: case 4: J |= VBA_BUTTON_R | VBA_BUTTON_B; MissileCount++; break;
		case 5: MissileCount = 0; break;
	}

	switch (Morph) {
		case 1: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_DOWN; Morph = 2; break;
		case 2: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); Morph = 3; break;
		case 3: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_DOWN; Morph = 0; break;
		case -1: case -2: case -3: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_UP; Morph--; break;
		case -4: case -5: case -6: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); Morph--; break;
		case -7: case -8: case -9: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_UP; Morph--; break;
		case -10: Morph = 0; break;
	}

	return J;
}

uint32_t MetroidFusionInput(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	u8 BallState = CPUReadByte(0x3001329); // 0 = stand, 2 = crouch, 5 = ball
	u16 Health = CPUReadHalfWord(0x3001310); 
	static u16 OldHealth = 0;

	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;
	static int AimCount = 0;
	static int MissileCount = 0;

	if (BallState == 5) J &= ~VBA_UP;
	if (BallState == 2) J &= ~VBA_DOWN;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if ((data.buttons_h & GUI_BTN_A) && BallState != 5) J |= VBA_BUTTON_A;
	else if (BallState == 5 && fabs(data.gforceY) > 1.5f) J |= VBA_BUTTON_A;

	if (data.buttons_h & GUI_BTN_B) J |= VBA_BUTTON_B;

	if (data.buttons_h & GUI_TRIGGER_L) J |= VBA_BUTTON_L;
	if (data.buttons_h & GUI_TRIGGER_R) J |= VBA_BUTTON_R;

	if (data.pitch < -45 && BallState != 5) {
		J |= VBA_UP;
		AimCount = 0;
	} else if (data.pitch < -22 && BallState != 5) {
		if (AimCount >= 0) AimCount = -1;
	} else if (data.pitch > 45 && BallState != 5) {
		if (AimCount < 10) AimCount = 10;
	} else if (data.pitch > 22 && BallState != 5) {
		if (AimCount <= 0 || AimCount >= 10) AimCount = 1;
	} else {
		AimCount = 0;
	}

	if (data.buttons_h & (GUI_BTN_X | GUI_BTN_Y)) MissileCount = 1;

	if (data.buttons_h & GUI_TRIGGER_ZL) {
		if (BallState == 5) Morph = -1;
		else if (BallState == 2) Morph = 2;
		else Morph = 1;
	}

	switch (AimCount) {
		case 1:
			J &= ~(VBA_UP | VBA_DOWN | VBA_BUTTON_L);
			J |= VBA_BUTTON_L;
			AimCount++;
			break;
		case 2:
			J &= ~(VBA_UP | VBA_DOWN | VBA_BUTTON_L);
			J |= VBA_BUTTON_L | VBA_DOWN;
			AimCount++;
			break;
		case 3:
			J |= VBA_BUTTON_L;
			break;
		case -1:
			J &= ~(VBA_UP | VBA_DOWN | VBA_BUTTON_L);
			J |= 0;
			AimCount--;
			break;
		case -2:
			J &= ~(VBA_UP | VBA_DOWN | VBA_BUTTON_L);
			J |= VBA_BUTTON_L;
			AimCount--;
			break;
		case -3:
			J |= VBA_BUTTON_L;
			break;
		case 10:
		case 11:
			J |= VBA_BUTTON_A;
			AimCount++;
			break;
		case 12:
		case 13:
			J |= VBA_DOWN;
			AimCount++;
			break;
		case 14:
			J |= VBA_DOWN;
			break;
	}

	switch (MissileCount) {
		case 1: case 2: J |= VBA_BUTTON_R; MissileCount++; break;
		case 3: case 4: J |= VBA_BUTTON_R | VBA_BUTTON_B; MissileCount++; break;
		case 5: MissileCount = 0; break;
	}

	switch (Morph) {
		case 1: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_DOWN; Morph = 2; break;
		case 2: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); Morph = 3; break;
		case 3: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_DOWN; Morph = 0; break;
		case -1: case -2: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_UP; Morph--; break;
		case -3: case -4: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); Morph--; break;
		case -5: case -6: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_UP; Morph--; break;
		case -7: Morph = 0; break;
	}

	return J;
}

uint32_t Metroid1Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	u8 BallState = CPUReadByte(0x3007500); // 3 = ball, other = stand
	u8 MissileState = CPUReadByte(0x300730E); // 1 = missile, 0 = beam
	u16 Health = CPUReadHalfWord(0x3007306);
	static u16 OldHealth = 0;

	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;

	if (BallState == 3) J &= ~VBA_UP;
	if (BallState != 3) J &= ~VBA_DOWN;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if ((data.buttons_h & GUI_BTN_A) && BallState != 5) J |= VBA_BUTTON_A;
	else if (BallState == 5 && fabs(data.gforceY) > 1.5f) J |= VBA_BUTTON_A;

	if (data.buttons_h & GUI_BTN_B) {
		if (MissileState) J |= VBA_BUTTON_SELECT;
		else J |= VBA_BUTTON_B;
	}

	if (data.buttons_h & (GUI_BTN_X | GUI_BTN_Y | GUI_TRIGGER_R)) {
		if (!MissileState) J |= VBA_BUTTON_SELECT;
		else J |= VBA_BUTTON_B;
	}
	
	if (data.buttons_h & GUI_TRIGGER_L) J |= VBA_UP; // Aim Up natively
	if (data.pitch < -45 && BallState != 3) J |= VBA_UP; // Aim Up tilt

	if (data.buttons_h & GUI_TRIGGER_ZL) {
		if (BallState == 3) Morph = -1;
		else Morph = 2;
	}

	switch (Morph) {
		case 1: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_DOWN; Morph = 2; break;
		case 2: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); Morph = 3; break;
		case 3: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_DOWN; Morph = 0; break;
		case -1: case -2: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_UP; Morph--; break;
		case -3: case -4: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); Morph--; break;
		case -5: case -6: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_UP; Morph--; break;
		case -7: Morph = 0; break;
	}

	return J;
}

uint32_t Metroid2Input(unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = StandardMovement(pad);

	u8 BallState = gbReadMemory(0xD020); // 4 = crouch, 5 = ball, other = stand
	u8 MissileState = gbReadMemory(0xD04D); // 8 = missile hatch open
	u8 Health = gbReadMemory(0xD051);
	static u8 OldHealth = 0;

	if (Health != OldHealth) systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;

	static int AimCount = 0;

	if (BallState == 5) J &= ~VBA_UP;
	if (BallState == 4) J &= ~VBA_DOWN;

	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;

	if ((data.buttons_h & GUI_BTN_A) && BallState != 5) J |= VBA_BUTTON_A;
	else if (BallState == 5 && fabs(data.gforceY) > 1.5f) J |= VBA_BUTTON_A;

	if (data.buttons_h & GUI_BTN_B) {
		if (MissileState & 8) J |= VBA_BUTTON_SELECT;
		else J |= VBA_BUTTON_B;
	}
	
	if (data.buttons_h & (GUI_BTN_X | GUI_BTN_Y | GUI_TRIGGER_R)) {
		if (!(MissileState & 8)) J |= VBA_BUTTON_SELECT;
		else J |= VBA_BUTTON_B;
	}

	if (data.buttons_h & GUI_TRIGGER_L) J |= VBA_UP;

	if (data.pitch < -45 && BallState != 5) {
		J |= VBA_UP;
		AimCount = 0;
	} else if (data.pitch > 45 && BallState != 5 && BallState != 4) {
		// Retained legacy structure
	} else {
		AimCount = 0;
	}

	if (data.buttons_h & GUI_TRIGGER_ZL) {
		if (BallState == 5) Morph = -1;
		else if (BallState == 4) Morph = 2;
		else Morph = 1;
	}

	switch (Morph) {
		case 1: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_DOWN; Morph = 2; break;
		case 2: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); Morph = 3; break;
		case 3: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_DOWN; Morph = 0; break;
		case -1: case -2: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_UP; Morph--; break;
		case -3: case -4: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); Morph--; break;
		case -5: case -6: J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L); J |= VBA_UP; Morph--; break;
		case -7: Morph = 0; break;
	}

	return J;
}

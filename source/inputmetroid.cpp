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

namespace {

// What each kind of controller is holding. The Wii Remote (with or without a Nunchuk) has its own scheme in these games,
// described in the README; the Classic Controller, Wii U Pro Controller, GamePad and GameCube pad play by another.
struct MetroidPads {
	uint32_t wm;		// the Wii Remote's own buttons (D-pad, A, B, 1, 2, + and -), held
	uint32_t wmUp;		// ... released since the last frame
	uint32_t nunchuk;	// the Nunchuk's C and Z, held
	bool morph;			// the Nunchuk's C, pressed since the last frame
	float pitch;		// the Wii Remote's pitch, and
	float gforceY;		// its acceleration up and down
	uint32_t classic;	// the Classic Controller, Pro Controller and GamePad, held
	uint32_t gc;		// the GameCube pad, held
	uint32_t gcDown;	// ... pressed since the last frame
};

MetroidPads ReadMetroidPads(const InputPadData& data)
{
	MetroidPads p;
	p.wm = p.wmUp = p.nunchuk = p.classic = p.gc = p.gcDown = 0;
	p.morph = false;
	p.pitch = p.gforceY = 0.0f;
	if (data.hw_connected[INPUT_HW_WIIMOTE])
	{
		p.wm = data.hw_buttons_h[INPUT_HW_WIIMOTE];
		p.wmUp = data.hw_buttons_r[INPUT_HW_WIIMOTE];
		p.pitch = data.hw_pitch[INPUT_HW_WIIMOTE];
		p.gforceY = data.hw_gforceY[INPUT_HW_WIIMOTE];
	}
	if (data.hw_connected[INPUT_HW_NUNCHUK])
	{
		p.nunchuk = data.hw_buttons_h[INPUT_HW_NUNCHUK];
		p.morph = (data.hw_buttons_d[INPUT_HW_NUNCHUK] & INPUT_TRIGGER_L) != 0;
	}
	p.classic = ClassicStyleHeld(data);
	if (data.hw_connected[INPUT_HW_GAMECUBE])
	{
		p.gc = data.hw_buttons_h[INPUT_HW_GAMECUBE];
		p.gcDown = data.hw_buttons_d[INPUT_HW_GAMECUBE];
	}
	return p;
}

// The D-pad of the pads that are not a Wii Remote: it moves. On a Wii Remote the D-pad does other things.
uint32_t MetroidPadDPad(const MetroidPads& p)
{
	const uint32_t held = p.classic | p.gc;
	uint32_t J = 0;
	if (held & INPUT_BTN_UP) J |= VBA_UP;
	if (held & INPUT_BTN_DOWN) J |= VBA_DOWN;
	if (held & INPUT_BTN_LEFT) J |= VBA_LEFT;
	if (held & INPUT_BTN_RIGHT) J |= VBA_RIGHT;
	return J;
}

} // namespace

uint32_t MetroidZeroInput(unsigned short pad) {
	if (!controller[pad]) return 0;
	const InputPadData& data = controller[pad]->getPadData();
	const MetroidPads p = ReadMetroidPads(data);
	uint32_t J = StandardMovement(pad) | MetroidPadDPad(p);

	uint8_t BallState = CPUReadByte(0x30015df); // 0 = stand, 1 = crouch, 2 = ball
	uint16_t Health = CPUReadByte(0x3001536);
	static uint16_t OldHealth = 0;

	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;
	static int AimCount = 0;
	static int MissileCount = 0;

	if (BallState == 2) J &= ~VBA_UP;
	if (BallState == 1) J &= ~VBA_DOWN;

	// Wii Remote, with or without a Nunchuk
	bool MorphBallButton = p.morph;	// the Nunchuk's C
	if (p.wm & INPUT_BTN_UP) J |= VBA_SPEED;
	if (p.wm & INPUT_BTN_LEFT) J |= VBA_BUTTON_L;
	if (p.wm & INPUT_BTN_RIGHT) J |= VBA_BUTTON_R;
	// Visor doesn't exist, use as start button
	if (p.wm & INPUT_BTN_MINUS) J |= VBA_BUTTON_START;
	// Hyper (toggle super missiles)
	if (p.wm & INPUT_BTN_PLUS) J |= VBA_BUTTON_SELECT;
	// Map
	if (p.wm & INPUT_BTN_1) J |= VBA_BUTTON_START;
	// Hint
	if (p.wm & INPUT_BTN_2) J |= VBA_BUTTON_R;
	// Jump (with accelerometer support for ball jump)
	if ((p.wm & INPUT_BTN_B) && BallState != 2) J |= VBA_BUTTON_A;
	else if (BallState == 2 && fabsf(p.gforceY) > 1.5f) J |= VBA_BUTTON_A;
	// Fire
	if (p.wm & INPUT_BTN_A) J |= VBA_BUTTON_B;
	// Missile
	if (p.wm & INPUT_BTN_DOWN) MissileCount = 1;
	// Aiming (pitch based)
	if (p.pitch < -45 && BallState != 2) {
		J |= VBA_UP;
		AimCount = 0;
	} else if (p.pitch < -22 && BallState != 2) {
		if (AimCount >= 0) AimCount = -1;
	} else if (p.pitch > 45 && BallState == 0) {
		if (AimCount < 10) AimCount = 10;
	} else if (p.pitch > 22 && BallState != 2) {
		if (AimCount <= 0 || AimCount >= 10) AimCount = 1;
	} else {
		AimCount = 0;
	}
	// Classic Controller, Pro Controller and GamePad
	if (p.classic & INPUT_BTN_PLUS) J |= VBA_BUTTON_START;
	if (p.classic & INPUT_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	if ((p.classic & INPUT_BTN_A) && BallState != 2) J |= VBA_BUTTON_A;
	if (p.classic & INPUT_BTN_B) J |= VBA_BUTTON_B;
	if (p.classic & INPUT_TRIGGER_L) J |= VBA_BUTTON_L;
	if (p.classic & INPUT_TRIGGER_R) J |= VBA_BUTTON_R;
	if (p.classic & (INPUT_BTN_X | INPUT_BTN_Y)) MissileCount = 1;
	if (p.classic & (INPUT_TRIGGER_ZL | INPUT_TRIGGER_ZR)) MorphBallButton = true;
	// GameCube pad
	if (p.gc & INPUT_BTN_A) J |= VBA_BUTTON_B;			// shoot beam
	if (p.gc & INPUT_BTN_B) J |= VBA_BUTTON_A;			// jump
	if (p.gc & INPUT_BTN_Y) MissileCount = 1;			// missile
	if (p.gc & INPUT_TRIGGER_ZR) J |= VBA_BUTTON_START;	// map
	if (p.gc & INPUT_TRIGGER_L) J |= VBA_BUTTON_L;		// aim up/down
	if (p.gc & INPUT_BTN_PLUS) J |= VBA_BUTTON_SELECT;	// toggle super missiles
	if (p.gc & INPUT_TRIGGER_R) J |= VBA_BUTTON_R;
	if (p.gcDown & INPUT_BTN_X) MorphBallButton = true;	// morph ball
	if (MorphBallButton) {
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
	if (!controller[pad]) return 0;
	const InputPadData& data = controller[pad]->getPadData();
	const MetroidPads p = ReadMetroidPads(data);
	uint32_t J = StandardMovement(pad) | MetroidPadDPad(p);

	uint8_t BallState = CPUReadByte(0x3001329); // 0 = stand, 2 = crouch, 5 = ball
	uint16_t Health = CPUReadHalfWord(0x3001310); 
	static uint16_t OldHealth = 0;

	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;
	static int AimCount = 0;
	static int MissileCount = 0;

	if (BallState == 5) J &= ~VBA_UP;
	if (BallState == 2) J &= ~VBA_DOWN;

	// Wii Remote, with or without a Nunchuk
	bool MorphBallButton = p.morph;	// the Nunchuk's C
	if (p.wm & INPUT_BTN_LEFT) J |= VBA_BUTTON_L;
	if (p.wm & INPUT_BTN_RIGHT) J |= VBA_BUTTON_R;
	// Visor doesn't exist, just use as select button
	if (p.wm & INPUT_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	// Hyper isn't available, just use start button as start
	if (p.wm & INPUT_BTN_PLUS) J |= VBA_BUTTON_START;
	if (p.wm & INPUT_BTN_1) J |= VBA_BUTTON_START;
	if (p.wm & INPUT_BTN_2) J |= VBA_BUTTON_R;
	if (p.nunchuk & INPUT_TRIGGER_ZL) J |= VBA_SPEED;
	if ((p.wm & INPUT_BTN_B) && BallState != 5) J |= VBA_BUTTON_A;
	else if (BallState == 5 && fabsf(p.gforceY) > 1.5f) J |= VBA_BUTTON_A;
	if (p.wm & INPUT_BTN_A) J |= VBA_BUTTON_B;
	if (p.wm & INPUT_BTN_DOWN) MissileCount = 1;
	if (p.pitch < -45 && BallState != 5) {
		J |= VBA_UP;
		AimCount = 0;
	} else if (p.pitch < -22 && BallState != 5) {
		if (AimCount >= 0) AimCount = -1;
	} else if (p.pitch > 45 && BallState != 5) {
		if (AimCount < 10) AimCount = 10;
	} else if (p.pitch > 22 && BallState != 5) {
		if (AimCount <= 0 || AimCount >= 10) AimCount = 1;
	} else {
		AimCount = 0;
	}
	// Classic Controller, Pro Controller, GamePad and GameCube pad
	const uint32_t pads = p.classic | p.gc;
	if (pads & INPUT_BTN_PLUS) J |= VBA_BUTTON_START;
	if (p.classic & INPUT_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	if ((pads & INPUT_BTN_A) && BallState != 5) J |= VBA_BUTTON_A;
	if (pads & INPUT_BTN_B) J |= VBA_BUTTON_B;
	if (pads & INPUT_TRIGGER_L) J |= VBA_BUTTON_L;
	if (pads & INPUT_TRIGGER_R) J |= VBA_BUTTON_R;
	if (pads & (INPUT_BTN_X | INPUT_BTN_Y)) MissileCount = 1;
	if (pads & (INPUT_TRIGGER_ZL | INPUT_TRIGGER_ZR)) MorphBallButton = true;
	if (MorphBallButton) {
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
	if (!controller[pad]) return 0;
	const InputPadData& data = controller[pad]->getPadData();
	const MetroidPads p = ReadMetroidPads(data);
	uint32_t J = StandardMovement(pad) | MetroidPadDPad(p);

	uint8_t BallState = CPUReadByte(0x3007500); // 3 = ball, other = stand
	uint8_t MissileState = CPUReadByte(0x300730E); // 1 = missile, 0 = beam
	uint16_t Health = CPUReadHalfWord(0x3007306);
	static uint16_t OldHealth = 0;

	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;

	if (BallState == 3) J &= ~VBA_UP;
	if (BallState != 3) J &= ~VBA_DOWN;

	// Wii Remote, with or without a Nunchuk
	bool MorphBallButton = p.morph;	// the Nunchuk's C
	if (p.wm & INPUT_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	if (p.wm & INPUT_BTN_PLUS) J |= VBA_BUTTON_START;
	// Map (doesn't exist)
	if (p.wm & INPUT_BTN_1) J |= VBA_BUTTON_START;
	// Hint (doesn't exist)
	if (p.wm & INPUT_BTN_2) J |= VBA_SPEED;
	if ((p.wm & INPUT_BTN_B) && BallState != 5) J |= VBA_BUTTON_A;
	else if (BallState == 5 && fabsf(p.gforceY) > 1.5f) J |= VBA_BUTTON_A;
	if (p.wm & INPUT_BTN_A) {
		if (MissileState) J |= VBA_BUTTON_SELECT;
		else J |= VBA_BUTTON_B;
	}
	if (p.pitch < -45 && BallState != 3) J |= VBA_UP;	// aim up
	if (p.wm & INPUT_BTN_DOWN) {
		if (!MissileState) J |= VBA_BUTTON_SELECT;
		else J |= VBA_BUTTON_B;
	}
	if ((p.wmUp & INPUT_BTN_DOWN) && MissileState) J |= VBA_BUTTON_SELECT;
	// Classic Controller, Pro Controller, GamePad and GameCube pad
	const uint32_t pads = p.classic | p.gc;
	if (pads & INPUT_BTN_PLUS) J |= VBA_BUTTON_START;
	if (p.classic & INPUT_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	if ((pads & INPUT_BTN_A) && BallState != 5) J |= VBA_BUTTON_A;
	if (pads & INPUT_BTN_B) {
		if (MissileState) J |= VBA_BUTTON_SELECT;
		else J |= VBA_BUTTON_B;
	}
	if (pads & (INPUT_BTN_X | INPUT_BTN_Y | INPUT_TRIGGER_R)) {
		if (!MissileState) J |= VBA_BUTTON_SELECT;
		else J |= VBA_BUTTON_B;
	}
	if (pads & INPUT_TRIGGER_L) J |= VBA_UP;	// aim up
	if (pads & (INPUT_TRIGGER_ZL | INPUT_TRIGGER_ZR)) MorphBallButton = true;
	if (MorphBallButton) {
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
	if (!controller[pad]) return 0;
	const InputPadData& data = controller[pad]->getPadData();
	const MetroidPads p = ReadMetroidPads(data);
	uint32_t J = StandardMovement(pad) | MetroidPadDPad(p);

	uint8_t BallState = gbReadMemory(0xD020); // 4 = crouch, 5 = ball, other = stand
	uint8_t MissileState = gbReadMemory(0xD04D); // 8 = missile hatch open
	uint8_t Health = gbReadMemory(0xD051);
	static uint8_t OldHealth = 0;

	if (Health != OldHealth) systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;

	static int AimCount = 0;

	if (BallState == 5) J &= ~VBA_UP;
	if (BallState == 4) J &= ~VBA_DOWN;

	// Wii Remote, with or without a Nunchuk
	bool MorphBallButton = p.morph;	// the Nunchuk's C
	if (p.wm & INPUT_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	if (p.wm & INPUT_BTN_PLUS) J |= VBA_BUTTON_START;
	// Map (doesn't exist)
	if (p.wm & INPUT_BTN_1) J |= VBA_BUTTON_START;
	// Hint (doesn't exist)
	if (p.wm & INPUT_BTN_2) J |= VBA_SPEED;
	if ((p.wm & INPUT_BTN_B) && BallState != 5) J |= VBA_BUTTON_A;
	else if (BallState == 5 && fabsf(p.gforceY) > 1.5f) J |= VBA_BUTTON_A;
	if (p.wm & INPUT_BTN_A) {
		if (MissileState & 8) J |= VBA_BUTTON_SELECT;
		else J |= VBA_BUTTON_B;
	}
	if (p.pitch < -45 && BallState != 5) {
		J |= VBA_UP;
		AimCount = 0;
	} else {
		AimCount = 0;
	}
	if (p.wm & INPUT_BTN_DOWN) {
		if (!(MissileState & 8)) J |= VBA_BUTTON_SELECT;
		else J |= VBA_BUTTON_B;
	}
	if ((p.wmUp & INPUT_BTN_DOWN) && (MissileState & 8)) J |= VBA_BUTTON_SELECT;
	// Classic Controller, Pro Controller, GamePad and GameCube pad
	const uint32_t pads = p.classic | p.gc;
	if (pads & INPUT_BTN_PLUS) J |= VBA_BUTTON_START;
	if (p.classic & INPUT_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	if ((pads & INPUT_BTN_A) && BallState != 5) J |= VBA_BUTTON_A;
	if (pads & INPUT_BTN_B) {
		if (MissileState & 8) J |= VBA_BUTTON_SELECT;
		else J |= VBA_BUTTON_B;
	}
	if (pads & (INPUT_BTN_X | INPUT_BTN_Y | INPUT_TRIGGER_R)) {
		if (!(MissileState & 8)) J |= VBA_BUTTON_SELECT;
		else J |= VBA_BUTTON_B;
	}
	if (pads & INPUT_TRIGGER_L) J |= VBA_UP;
	if (pads & (INPUT_TRIGGER_ZL | INPUT_TRIGGER_ZR)) MorphBallButton = true;
	if (MorphBallButton) {
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

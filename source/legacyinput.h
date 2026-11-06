/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * legacyinput.h
 *
 * The Wii Remote, Classic Controller and GameCube pad as the per-game
 * "Match Wii Controls" handlers (gameinput.cpp, inputzelda.cpp, ...) read
 * them before the platform-agnostic input refactor, rebuilt each time from
 * the platform-agnostic InputPadData. That lets those handlers keep the
 * logic they were written with. The names mirror libogc's (wpad.h, pad.h) with
 * an LW_ / LP_ prefix, since libogc itself is not available on every platform.
 *
 * On every platform the Classic Controller, Wii U Pro Controller and GamePad
 * are one thing here, a Classic Controller; a Wii Remote with a Nunchuk is a
 * Nunchuk; the Wii Remote on its own has no expansion.
 ***************************************************************************/

#pragma once

#include <stdint.h>

// Wii Remote
#define LW_BUTTON_2			0x0001
#define LW_BUTTON_1			0x0002
#define LW_BUTTON_B			0x0004
#define LW_BUTTON_A			0x0008
#define LW_BUTTON_MINUS		0x0010
#define LW_BUTTON_HOME		0x0080
#define LW_BUTTON_LEFT		0x0100
#define LW_BUTTON_RIGHT		0x0200
#define LW_BUTTON_DOWN		0x0400
#define LW_BUTTON_UP		0x0800
#define LW_BUTTON_PLUS		0x1000

// Nunchuk
#define LW_NUNCHUK_BUTTON_Z	(0x0001u << 16)
#define LW_NUNCHUK_BUTTON_C	(0x0002u << 16)

// Classic Controller
#define LW_CLASSIC_BUTTON_UP		(0x0001u << 16)
#define LW_CLASSIC_BUTTON_LEFT		(0x0002u << 16)
#define LW_CLASSIC_BUTTON_ZR		(0x0004u << 16)
#define LW_CLASSIC_BUTTON_X			(0x0008u << 16)
#define LW_CLASSIC_BUTTON_A			(0x0010u << 16)
#define LW_CLASSIC_BUTTON_Y			(0x0020u << 16)
#define LW_CLASSIC_BUTTON_B			(0x0040u << 16)
#define LW_CLASSIC_BUTTON_ZL		(0x0080u << 16)
#define LW_CLASSIC_BUTTON_FULL_R	(0x0200u << 16)
#define LW_CLASSIC_BUTTON_PLUS		(0x0400u << 16)
#define LW_CLASSIC_BUTTON_HOME		(0x0800u << 16)
#define LW_CLASSIC_BUTTON_MINUS		(0x1000u << 16)
#define LW_CLASSIC_BUTTON_FULL_L	(0x2000u << 16)
#define LW_CLASSIC_BUTTON_DOWN		(0x4000u << 16)
#define LW_CLASSIC_BUTTON_RIGHT		(0x8000u << 16)

// What is plugged into the Wii Remote
#define LW_EXP_NONE			0
#define LW_EXP_NUNCHUK		1
#define LW_EXP_CLASSIC		2

struct LegacyVec3 { float x, y, z; };
struct LegacyOrient { float pitch, roll, yaw; };
struct LegacyStick { float mag; float x, y; };

struct LegacyWpad {
	uint32_t btns_h, btns_d, btns_u;	//!< held, pressed and released this frame, with the expansion's buttons in the upper half
	LegacyVec3 gforce;
	LegacyOrient orient;
	struct {
		int type;						//!< LW_EXP_*
		struct {
			LegacyVec3 gforce;
			LegacyOrient orient;
			LegacyStick js;
		} nunchuk;
		struct {
			LegacyStick ljs, rjs;
			int ls_raw, rs_raw;			//!< the analog triggers, 0 to 31
		} classic;
	} exp;
};

//! The Wii Remote of a channel, as the handlers read it; valid until the next call for that channel
const LegacyWpad * LegacyWpad_Data(unsigned short pad);
//! The Classic Controller's sticks, -128 to 127 (0 the left stick, 1 the right)
int8_t LegacyWpad_StickX(unsigned short pad, int stick);
int8_t LegacyWpad_StickY(unsigned short pad, int stick);

// GameCube pad
#define LP_BUTTON_LEFT		0x0001
#define LP_BUTTON_RIGHT		0x0002
#define LP_BUTTON_DOWN		0x0004
#define LP_BUTTON_UP		0x0008
#define LP_TRIGGER_Z		0x0010
#define LP_TRIGGER_R		0x0020
#define LP_TRIGGER_L		0x0040
#define LP_BUTTON_A			0x0100
#define LP_BUTTON_B			0x0200
#define LP_BUTTON_X			0x0400
#define LP_BUTTON_Y			0x0800
#define LP_BUTTON_START		0x1000

uint32_t LegacyPad_ButtonsHeld(unsigned short pad);
uint32_t LegacyPad_ButtonsDown(unsigned short pad);
uint32_t LegacyPad_ButtonsUp(unsigned short pad);	// released this frame
int8_t LegacyPad_StickX(unsigned short pad);
int8_t LegacyPad_StickY(unsigned short pad);
int8_t LegacyPad_SubStickX(unsigned short pad);
int8_t LegacyPad_SubStickY(unsigned short pad);

//! What the user's own Button Mappings make of each kind of hardware, as Game Boy Advance buttons: the baseline the
//! handlers add their own buttons to
uint32_t DecodeGamecube(unsigned short pad);
uint32_t DecodeWiimote(unsigned short pad);	// only a Wii Remote on its own
uint32_t DecodeClassic(unsigned short pad);	// Classic Controller, Pro Controller and GamePad

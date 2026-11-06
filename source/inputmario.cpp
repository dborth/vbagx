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

uint32_t MarioKartInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad);
	static uint32_t frame = 0;
	const LegacyWpad * wp = LegacyWpad_Data(pad);

	uint8_t Health = 0;
	static uint8_t OldHealth = 0;
	float fraction;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	// Start/Select
	if (wp->btns_h & LW_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	if (wp->btns_h & LW_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	if (wp->exp.type == LW_EXP_NONE)
	{
		// Use item
		if (wp->btns_h & LW_BUTTON_RIGHT)
		{
			J |= VBA_BUTTON_L | VBA_UP;
			J &= ~VBA_DOWN;
		}
		else if (wp->btns_h & LW_BUTTON_LEFT)
		{
			J |= VBA_BUTTON_L | VBA_DOWN;
			J &= ~VBA_UP;
		}
		else if (wp->btns_h & LW_BUTTON_UP || wp->btns_h & LW_BUTTON_DOWN)
			J |= VBA_BUTTON_L;
		// Accelerate
		if (wp->btns_h & LW_BUTTON_2)
			J |= VBA_BUTTON_A;
		// Brake
		if (wp->btns_h & LW_BUTTON_1)
			J |= VBA_BUTTON_B;
		// Jump/Power slide
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_BUTTON_R;
		if (wp->orient.pitch> 12) {
			fraction = (wp->orient.pitch - 12)/60.0f;
			if ((frame % 60)/60.0f < fraction)
				J |= VBA_LEFT;
		} else if (wp->orient.pitch <- 12) {
			fraction = -(wp->orient.pitch + 10)/60.0f;
			if ((frame % 60)/60.0f < fraction)
				J |= VBA_RIGHT;
		}
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		// Use item
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z)
			J |= VBA_BUTTON_L;
		// Accelerate
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Brake
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Jump
		if (fabs(wp->gforce.y)> 1.5 )
			J |= VBA_BUTTON_R;
		if (wp->btns_h & LW_BUTTON_UP || wp->btns_h & LW_BUTTON_LEFT || wp->btns_h & LW_BUTTON_RIGHT || wp->btns_h & LW_BUTTON_DOWN)
			J |= VBA_BUTTON_R;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		// Use item
		if (wp->btns_h & LW_CLASSIC_BUTTON_FULL_L)
			J |= VBA_BUTTON_L;
		// Accelerate
		if (wp->btns_h & LW_CLASSIC_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Brake
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Jump
		if (wp->btns_h & LW_CLASSIC_BUTTON_FULL_R)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & LW_CLASSIC_BUTTON_UP || wp->btns_h & LW_CLASSIC_BUTTON_LEFT || wp->btns_h & LW_CLASSIC_BUTTON_RIGHT || wp->btns_h & LW_CLASSIC_BUTTON_DOWN)
			J |= VBA_BUTTON_R;
		// Start/Select
		if (wp->btns_h & LW_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	}

	uint32_t gc = LegacyPad_ButtonsHeld(pad);
	if (gc & LP_BUTTON_START)
		J |= VBA_BUTTON_START;
	if (gc & LP_BUTTON_X || gc & LP_BUTTON_Y)
		J |= VBA_BUTTON_SELECT;
	if (gc & LP_BUTTON_A)
		J |= VBA_BUTTON_A;
	if (gc & LP_BUTTON_B)
		J |= VBA_BUTTON_B;
	if (gc & LP_TRIGGER_L)
		J |= VBA_BUTTON_L;
	if (gc & LP_TRIGGER_R)
		J |= VBA_BUTTON_R;
	if (gc & LP_BUTTON_UP || gc & LP_BUTTON_DOWN || gc & LP_BUTTON_LEFT
			|| gc & LP_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;

	frame++;

	return J;
}

uint32_t Mario1DXInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);

	bool run = false;
	run = (wp->exp.type==LW_EXP_NUNCHUK && wp->exp.nunchuk.js.mag> 0.83) || (wp->exp.type==LW_EXP_CLASSIC && wp->exp.classic.ljs.mag> 0.83);
	run = run && ((J & VBA_LEFT) || (J & VBA_RIGHT));

	if (wp->exp.type==LW_EXP_NUNCHUK) {
		// Pause
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;

		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Run, pick up
		if (wp->btns_h & LW_BUTTON_B || run)
			J |= VBA_BUTTON_B;
		// Starspin shoots when using fireflower
		if (fabs(wp->gforce.x)> 1.5)
			J |= VBA_BUTTON_B;

		// Camera (must come before Crouch)
		if (!(wp->btns_h & LW_NUNCHUK_BUTTON_C))
			J &= ~(VBA_DOWN | VBA_UP);
		if (wp->btns_h & LW_BUTTON_UP)
			J |= VBA_UP;
		if (wp->btns_h & LW_BUTTON_DOWN)
			J |= VBA_DOWN;
		// Crouch (must come after Camera)
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z)
			J |= VBA_DOWN;

		// Speed
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		// Pause
		if (wp->btns_h & LW_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & LW_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & LW_CLASSIC_BUTTON_X || wp->btns_h & LW_CLASSIC_BUTTON_Y || run)
			J |= VBA_BUTTON_B;
		// Spin attack
		if (wp->btns_h & LW_CLASSIC_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Camera
		J &= ~(VBA_DOWN | VBA_UP);
		if (wp->btns_h & LW_CLASSIC_BUTTON_FULL_L)
			J |= VBA_DOWN;
		if (wp->btns_h & LW_CLASSIC_BUTTON_FULL_R)
			J |= VBA_UP;
		// Crouch
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZL) {
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}
		// DPad movement/camera
		if (wp->btns_h & LW_CLASSIC_BUTTON_UP)
			J |= VBA_UP;
		if (wp->btns_h & LW_CLASSIC_BUTTON_DOWN)
			J |= VBA_DOWN;
		if (wp->btns_h & LW_CLASSIC_BUTTON_LEFT)
			J |= VBA_LEFT;
		if (wp->btns_h & LW_CLASSIC_BUTTON_RIGHT)
			J |= VBA_RIGHT;
		// Speed
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZR)
			J |= VBA_SPEED;
	} else
		J |= StandardSideways(pad);
	return J;
}

uint32_t Mario1ClassicInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);

	bool run = false;

	run = (wp->exp.type==LW_EXP_NUNCHUK && wp->exp.nunchuk.js.mag> 0.83) || (wp->exp.type==LW_EXP_CLASSIC && wp->exp.classic.ljs.mag> 0.83);
	run = run && ((J & VBA_LEFT) || (J & VBA_RIGHT));

	if (wp->exp.type==LW_EXP_NUNCHUK) {
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;

		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Run, pick up
		if (wp->btns_h & LW_BUTTON_B || run)
			J |= VBA_BUTTON_B;
		// Starspin shoots when using fireflower
		if (fabs(wp->gforce.x)> 1.5)
			J |= VBA_BUTTON_B;

		// Crouch
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z) {
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}

		// Speed
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		// DPad movement
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & LW_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & LW_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & LW_CLASSIC_BUTTON_X || wp->btns_h & LW_CLASSIC_BUTTON_Y || run)
			J |= VBA_BUTTON_B;
		// Spin attack
		if (wp->btns_h & LW_CLASSIC_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Crouch
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZL || wp->btns_h & LW_CLASSIC_BUTTON_FULL_L)
		{
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}
		// Speed
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZR)
			J |= VBA_SPEED;
	} else
		J |= StandardSideways(pad);
	return J;
}

uint32_t MarioLand1Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	bool run = false;

	run = (wp->exp.type==LW_EXP_NUNCHUK && wp->exp.nunchuk.js.mag> 0.83) || (wp->exp.type==LW_EXP_CLASSIC && wp->exp.classic.ljs.mag> 0.83);
	run = run && ((J & VBA_LEFT) || (J & VBA_RIGHT));

	if (wp->exp.type == LW_EXP_NONE)
		J |= StandardSideways(pad);
	else if (wp->exp.type == LW_EXP_CLASSIC) {
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & LW_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & LW_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & LW_CLASSIC_BUTTON_X || wp->btns_h & LW_CLASSIC_BUTTON_Y || run)
			J |= VBA_BUTTON_B;
		// Spin attack
		if (wp->btns_h & LW_CLASSIC_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Crouch
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZL || wp->btns_h & LW_CLASSIC_BUTTON_FULL_L)
		{
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}
		// Speed
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZR || wp->btns_h & LW_CLASSIC_BUTTON_FULL_R)
			J |= VBA_SPEED;
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & LW_BUTTON_B || run)
			J |= VBA_BUTTON_B;
		if (fabs(wp->gforce.x)> 1.5)
			J |= VBA_BUTTON_B;
		// Crouch
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z)
			J |= VBA_DOWN;
		// Speed
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C || wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
	}
	else
		J |= DecodeWiimote(pad);

	return J;
}

uint32_t MarioLand2Input(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	bool run = false;

	run = (wp->exp.type==LW_EXP_NUNCHUK && wp->exp.nunchuk.js.mag> 0.83) || (wp->exp.type==LW_EXP_CLASSIC && wp->exp.classic.ljs.mag> 0.83);
	run = run && ((J & VBA_LEFT) || (J & VBA_RIGHT));

	if (wp->exp.type == LW_EXP_NONE)
		J |= StandardSideways(pad);
	else if (wp->exp.type == LW_EXP_CLASSIC) {
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & LW_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & LW_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & LW_CLASSIC_BUTTON_X || wp->btns_h & LW_CLASSIC_BUTTON_Y || run)
			J |= VBA_BUTTON_B;
		// Spin attack
		if (wp->btns_h & LW_CLASSIC_BUTTON_A || wp->btns_h & LW_CLASSIC_BUTTON_FULL_R)
			J |= VBA_DOWN | VBA_BUTTON_A;
		// Crouch
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZL && (!(wp->btns_h & LW_CLASSIC_BUTTON_A))) {
			J |= VBA_DOWN;
			// if the run button is held down, crouching also looks down
			// which we don't want when using the Z button
			J &= ~VBA_BUTTON_B;
			J &= ~VBA_UP;
		}
		// Speed
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZR)
			J |= VBA_SPEED;
	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		// Pause
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & LW_BUTTON_B || run)
			J |= VBA_BUTTON_B;
		// Spin attack
		if (fabs(wp->gforce.x)> 1.4)
			J |= VBA_DOWN | VBA_BUTTON_A;
		// Camera
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C)
			J |= VBA_BUTTON_B;
		if (wp->btns_h & LW_BUTTON_UP)
			J |= VBA_BUTTON_B | VBA_UP;
		if (wp->btns_h & LW_BUTTON_DOWN)
			J |= VBA_BUTTON_B | VBA_DOWN;
		if (wp->btns_h & LW_BUTTON_LEFT)
			J |= VBA_BUTTON_B | VBA_LEFT;
		if (wp->btns_h & LW_BUTTON_RIGHT)
			J |= VBA_BUTTON_B | VBA_RIGHT;
		// Crouch
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z && (!(wp->btns_h & LW_BUTTON_A))) {
			J |= VBA_DOWN;
			// if the run button is held down, crouching also looks down
			// which we don't want when using the Z button
			J &= ~VBA_BUTTON_B;
			J &= ~VBA_UP;
		}
		// Speed
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
	} else
		J |= DecodeWiimote(pad);
	return J;
}

uint32_t Mario2Input(unsigned short pad)
{
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeWiimote(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	bool run = false;
	run = (wp->exp.type==LW_EXP_NUNCHUK && wp->exp.nunchuk.js.mag> 0.83) || (wp->exp.type==LW_EXP_CLASSIC && wp->exp.classic.ljs.mag> 0.83);
	run = run && ((J & VBA_LEFT) || (J & VBA_RIGHT));

	if (wp->exp.type == LW_EXP_NUNCHUK) {
		// DPad looks around
		if (wp->btns_h & LW_BUTTON_RIGHT)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & LW_BUTTON_LEFT)
			J |= VBA_BUTTON_L;
		if (wp->btns_h & LW_BUTTON_UP)
			J |= VBA_BUTTON_L;
		if (wp->btns_h & LW_BUTTON_DOWN)
			J |= VBA_BUTTON_L;

		// Pause
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;

		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Run, pick up
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_BUTTON_B;
		if (fabs(wp->gforce.x)> 1.5)
			J |= VBA_BUTTON_B;

		// Crouch
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z)
			J |= VBA_DOWN;

		if (wp->btns_h & LW_NUNCHUK_BUTTON_C)
			J |= VBA_BUTTON_L;
		if (wp->btns_h & LW_BUTTON_1)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & LW_BUTTON_2)
			J |= VBA_BUTTON_R;
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		// DPad movement
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & LW_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & LW_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & LW_CLASSIC_BUTTON_X || wp->btns_h & LW_CLASSIC_BUTTON_Y || run)
			J |= VBA_BUTTON_B;
		// Spin attack
		if (wp->btns_h & LW_CLASSIC_BUTTON_A)
			J |= VBA_BUTTON_A;
		// R
		if (wp->btns_h & LW_CLASSIC_BUTTON_FULL_R)
			J |= VBA_BUTTON_R;
		// Crouch
		if (wp->btns_h & LW_CLASSIC_BUTTON_FULL_L || wp->btns_h & LW_CLASSIC_BUTTON_ZL) {
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}
		// Speed
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZR)
			J |= VBA_SPEED;
	}
	return J;
}

uint32_t MarioWorldInput(unsigned short pad) {
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	uint8_t CarryState = CPUReadByte(0x3003F06); // 01 = carrying, 00 = not carrying
	uint8_t InYoshisMouth = CPUReadByte(0x3003F53); // FF = nothing, 00 = no level, else thing
	uint8_t FallState = CPUReadByte(0x3003FA1); // 0B = jump, 24 = fall
	uint8_t RidingYoshi = CPUReadByte(0x3004302); // 00 = not riding, 01 = riding
	static bool NeedStomp = false;
	bool dontrun = false;
	bool run = false;
	static bool dontletgo = false;
	dontrun = (InYoshisMouth != 0 && InYoshisMouth != 0xFF && RidingYoshi);
	bool BButton = false;
	const LegacyWpad * wp = LegacyWpad_Data(pad);

	run = (wp->exp.type==LW_EXP_NUNCHUK && wp->exp.nunchuk.js.mag> 0.83) || (wp->exp.type==LW_EXP_CLASSIC && wp->exp.classic.ljs.mag> 0.83);
	BButton = (wp->exp.type == LW_EXP_CLASSIC && (wp->btns_h & LW_CLASSIC_BUTTON_X || wp->btns_h & LW_CLASSIC_BUTTON_Y)) || wp->btns_h & LW_BUTTON_B;
	uint32_t gc = LegacyPad_ButtonsHeld(pad);
	BButton = BButton || gc & LP_BUTTON_B;
	if (run && CarryState && !BButton)
		dontletgo = true;
	if (!CarryState)
		dontletgo = false;
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;

		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Run, pick up
		if ((run && !dontrun) || wp->btns_h & LW_BUTTON_B || dontletgo)
			J |= VBA_BUTTON_B;
		if (wp->btns_h & LW_BUTTON_B && dontletgo) {
			J = J & ~VBA_BUTTON_B;
			dontletgo = false;
		}
		// Spin attack
		if (fabs(wp->gforce.x)> 1.5)
			J |= VBA_BUTTON_R;

		// Butt stomp
		if (FallState!=0 && (wp->exp.type == LW_EXP_NUNCHUK) && (wp->btns_h & LW_NUNCHUK_BUTTON_Z) && (!RidingYoshi))
			NeedStomp = true;
		if (FallState!=0 && (wp->exp.type == LW_EXP_CLASSIC) && (wp->btns_h & LW_CLASSIC_BUTTON_ZL || wp->btns_h & LW_CLASSIC_BUTTON_ZR) && (!RidingYoshi))
			NeedStomp = true;
		// Crouch
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z) {
			J |= VBA_DOWN; // Crouch
			J &= ~VBA_UP;
		}

		// Camera
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C) {
			if (J & VBA_DOWN || J & VBA_UP)
				J |= VBA_BUTTON_L;
			else if (J & VBA_RIGHT || J & VBA_LEFT) {
				J |= VBA_BUTTON_L;
				J &= ~VBA_RIGHT;
				J &= ~VBA_LEFT;
			}
		}
	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & LW_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & LW_CLASSIC_BUTTON_X || wp->btns_h & LW_CLASSIC_BUTTON_Y)
			J |= VBA_BUTTON_B;
		if (wp->btns_h & LW_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		if (wp->btns_h & LW_CLASSIC_BUTTON_A || wp->btns_h & LW_CLASSIC_BUTTON_FULL_R)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & LW_CLASSIC_BUTTON_FULL_L)
			J |= VBA_BUTTON_L;
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZL || wp->btns_h & LW_CLASSIC_BUTTON_ZR)
		{
			J |= VBA_DOWN; // Crouch
			J &= ~VBA_UP;
		}
	}

	BButton = (wp->exp.type == LW_EXP_CLASSIC && (wp->btns_h & LW_CLASSIC_BUTTON_X || wp->btns_h & LW_CLASSIC_BUTTON_Y)) || wp->btns_h & LW_BUTTON_B;
	// Gamecube controller
	if (gc & LP_BUTTON_A)
		J |= VBA_BUTTON_A;
	if (gc & LP_BUTTON_B)
		J |= VBA_BUTTON_B;
	if (gc & LP_BUTTON_X)
		J |= VBA_BUTTON_R;
	if (gc & LP_BUTTON_Y)
		J |= VBA_BUTTON_SELECT;
	if (gc & LP_TRIGGER_R)
		J |= VBA_BUTTON_R;
	if (gc & LP_TRIGGER_L)
		J |= VBA_BUTTON_L;
	if (gc & LP_BUTTON_START)
		J |= VBA_BUTTON_START;
	if (gc & LP_TRIGGER_Z) {
		J |= VBA_DOWN;
		J &= ~VBA_UP;
	}
	// if we try to use Yoshi's tongue while running, release run for one frame
	if (run && BButton && !dontrun && !dontletgo)
		J &= ~VBA_BUTTON_B;
	if (NeedStomp && FallState == 0 && !RidingYoshi) {
		J |= VBA_BUTTON_R; // spin attack only works when on ground
		NeedStomp = false;
	}

	return J;
}

uint32_t Mario3Input(unsigned short pad)
{
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	bool run = false;

	run = (wp->exp.type==LW_EXP_NUNCHUK && wp->exp.nunchuk.js.mag> 0.83) || (wp->exp.type==LW_EXP_CLASSIC && wp->exp.classic.ljs.mag> 0.83);
	run = run && ((J & VBA_LEFT) || (J & VBA_RIGHT));

	if (wp->exp.type == LW_EXP_NONE)
		J |= DecodeWiimote(pad);
	else if (wp->exp.type == LW_EXP_CLASSIC) {
		// DPad movement
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & LW_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_L;
		// Replay menu
		if (wp->btns_h & LW_CLASSIC_BUTTON_FULL_R)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & LW_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & LW_CLASSIC_BUTTON_X || wp->btns_h & LW_CLASSIC_BUTTON_Y || run)
			J |= VBA_BUTTON_B;
		// Spin attack (racoon costume, fire flower)
		if (wp->btns_h & LW_CLASSIC_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Crouch
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZL || wp->btns_h & LW_CLASSIC_BUTTON_FULL_L) {
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}
		// Speed
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZR)
			J |= VBA_SPEED;

	} else if (wp->exp.type == LW_EXP_NUNCHUK) {
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_L;
		// Replay Menu
		if (wp->btns_h & LW_BUTTON_1)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & LW_BUTTON_B || run)
			J |= VBA_BUTTON_B;
		// Spin attack (racoon, fire flower)
		if (fabs(wp->gforce.x)> 1.4)
			J |= VBA_BUTTON_B;
		// Crouch
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z)
		{
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}
		// Speed
		if (wp->btns_h & LW_BUTTON_2)
			J |= VBA_SPEED;
	}

	return J;
}

uint32_t YoshiIslandInput(unsigned short pad)
{
	uint32_t J = StandardMovement(pad) | DecodeWiimote(pad) | DecodeGamecube(pad);

	const LegacyWpad * wp = LegacyWpad_Data(pad);
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;

		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Run, pick up
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_BUTTON_B;
		// throw
		if (wp->btns_h & LW_BUTTON_1 || wp->btns_h & LW_BUTTON_2)
			J |= VBA_BUTTON_R;

		// Crouch, lay egg
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z) {
			J |= VBA_DOWN; // Crouch
			J &= ~VBA_UP;
		}

		// Camera
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C)
			J |= VBA_BUTTON_L;

	} else if (wp->exp.type == LW_EXP_CLASSIC) {
		J |= StandardDPad(pad);
		if (wp->btns_h & LW_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & LW_CLASSIC_BUTTON_X)
			J |= VBA_BUTTON_B;
		if (wp->btns_h & LW_CLASSIC_BUTTON_Y)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & LW_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		if (wp->btns_h & LW_CLASSIC_BUTTON_A || wp->btns_h & LW_CLASSIC_BUTTON_FULL_R)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & LW_CLASSIC_BUTTON_FULL_L)
			J |= VBA_BUTTON_L;
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZL || wp->btns_h & LW_CLASSIC_BUTTON_ZR)
		{
			J |= VBA_DOWN; // Crouch
			J &= ~VBA_UP;
		}
	}
	// Gamecube controller
	{
		uint32_t gc = LegacyPad_ButtonsHeld(pad);
		if (gc & LP_BUTTON_A)
			J |= VBA_BUTTON_A;
		if (gc & LP_BUTTON_B)
			J |= VBA_BUTTON_B;
		if (gc & LP_BUTTON_X)
			J |= VBA_BUTTON_R;
		if (gc & LP_BUTTON_Y)
			J |= VBA_BUTTON_SELECT;
		if (gc & LP_TRIGGER_R)
			J |= VBA_BUTTON_R;
		if (gc & LP_TRIGGER_L)
			J |= VBA_BUTTON_L;
		if (gc & LP_BUTTON_START)
			J |= VBA_BUTTON_START;
		if (gc & LP_TRIGGER_Z) {
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}
	}
	return J;
}

uint32_t UniversalGravitationInput(unsigned short pad) {
	TiltScreen = true;
	uint32_t J = StandardMovement(pad) | DecodeGamecube(pad);
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	if (wp->exp.type == LW_EXP_NUNCHUK) {
		J |= StandardDPad(pad);
		TiltSideways = false;
		// Pause
		if (wp->btns_h & LW_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select, L & R do nothing!
		if (wp->btns_h & LW_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & LW_BUTTON_1)
			J |= VBA_BUTTON_L | VBA_SPEED;
		if (wp->btns_h & LW_BUTTON_2)
			J |= VBA_BUTTON_R | VBA_SPEED;

		// Jump
		if (wp->btns_h & LW_BUTTON_A)
			J |= VBA_BUTTON_A;
		// tongue
		if (wp->btns_h & LW_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Crouch, butt stomp
		if (wp->btns_h & LW_NUNCHUK_BUTTON_Z) {
			J |= VBA_DOWN; // Crouch
			J &= ~VBA_UP;
		}
		// Camera
		if (wp->btns_h & LW_NUNCHUK_BUTTON_C) {
			J |= VBA_UP;
		}
	}
	else if (wp->exp.type == LW_EXP_CLASSIC) {
		TiltSideways = false;
		J |= StandardDPad(pad);
		// A bit stupid playing with the classic controller, but nevermind.
		if (wp->btns_h & LW_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & LW_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & LW_CLASSIC_BUTTON_X)
			J |= VBA_BUTTON_B;
		if (wp->btns_h & LW_CLASSIC_BUTTON_Y)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & LW_CLASSIC_BUTTON_B || wp->btns_h & LW_CLASSIC_BUTTON_A)
			J |= VBA_BUTTON_A;
		if (wp->btns_h & LW_CLASSIC_BUTTON_FULL_L || wp->btns_h & LW_CLASSIC_BUTTON_ZL)
			J |= VBA_DOWN;
		if (wp->btns_h & LW_CLASSIC_BUTTON_FULL_R)
			J |= VBA_UP;
		if (wp->btns_h & LW_CLASSIC_BUTTON_ZR)
			J |= VBA_SPEED;
	} else {
		J |= StandardSideways(pad);
		TiltSideways = true;
	}
	return J;
}

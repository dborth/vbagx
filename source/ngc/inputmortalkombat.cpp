/****************************************************************************
 * Visual Boy Advance GX
 *
 * Carl Kenner April 2009
 *
 * inputmortalkombat.cpp
 *
 * Wii/Gamecube controls for Mortal Kombat
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ogcsys.h>
#include <unistd.h>
#include <wiiuse/wpad.h>

#include "vba.h"
#include "button_mapping.h"
#include "audio.h"
#include "video.h"
#include "input.h"
#include "gameinput.h"
#include "vbasupport.h"
#include "wiiusbsupport.h"
#include "gba/GBA.h"
#include "gba/bios.h"
#include "gba/GBAinline.h"

u32 MK1Input(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeKeyboard(pad);
	u8 Health = gbReadMemory(0xc695);
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(5);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	// Punch
	if (wp->btns_h & WPAD_BUTTON_UP || wp->btns_h & WPAD_BUTTON_LEFT)
		J |= VBA_BUTTON_B;
	// Kick
	if (wp->btns_h & WPAD_BUTTON_DOWN || wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_A;
	// Block
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_BUTTON_A | VBA_BUTTON_B; // different from MK2
	// Throw
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_B | VBA_RIGHT; // CAKTODO check which way we are facing
	// Pause
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;
	// Start
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Special move
	if (wp->btns_h & WPAD_BUTTON_B)
	{
		// CAKTODO
	}
	
	if (wp->exp.type == WPAD_EXP_CLASSIC) {
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_UP) J |= VBA_UP;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_DOWN) J |= VBA_DOWN;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_LEFT) J |= VBA_LEFT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_RIGHT) J |= VBA_RIGHT;
		if (wp->btns_h & (WPAD_CLASSIC_BUTTON_X | WPAD_CLASSIC_BUTTON_Y)) J |= VBA_BUTTON_B;
		if (wp->btns_h & (WPAD_CLASSIC_BUTTON_A | WPAD_CLASSIC_BUTTON_B)) J |= VBA_BUTTON_A;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS) J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS) J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZR) J |= VBA_BUTTON_B | VBA_RIGHT; // CAKTODO
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R) J |= VBA_BUTTON_B | VBA_BUTTON_A; // block
	}

#endif
	{
		u32 gc = PAD_ButtonsHeld(pad);
		// DPad moves
		if (gc & PAD_BUTTON_UP) J |= VBA_UP;
		if (gc & PAD_BUTTON_DOWN) J |= VBA_DOWN;
		if (gc & PAD_BUTTON_LEFT) J |= VBA_LEFT;
		if (gc & PAD_BUTTON_RIGHT) J |= VBA_RIGHT;
		if (gc & PAD_BUTTON_START) J |= VBA_BUTTON_START;
		if (gc & (PAD_BUTTON_B | PAD_BUTTON_Y)) J |= VBA_BUTTON_B;
		if (gc & (PAD_BUTTON_A | PAD_BUTTON_X)) J |= VBA_BUTTON_A;
		if (gc & PAD_TRIGGER_Z) J |= VBA_BUTTON_B | VBA_RIGHT; // CAKTODO
		if (gc & PAD_TRIGGER_R) J |= VBA_BUTTON_B | VBA_BUTTON_A; // block
	}
	return J;
}

u32 MK4Input(unsigned short pad)
{
	u32 J = StandardMovement(pad) | DecodeKeyboard(pad);
	u8 Health = 0;
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	// Punch
	if (wp->btns_h & WPAD_BUTTON_UP || wp->btns_h & WPAD_BUTTON_LEFT)
		J |= VBA_BUTTON_B;
	// Kick
	if (wp->btns_h & WPAD_BUTTON_DOWN || wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_A;
	// Block
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_BUTTON_START;
	// Throw
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_B | VBA_RIGHT; // CAKTODO check which way we are facing
	// Pause
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;
	// Start
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Special move
	if (wp->btns_h & WPAD_BUTTON_B)
	{
		// CAKTODO
	}
	if (wp->exp.type == WPAD_EXP_CLASSIC) {
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_UP) J |= VBA_UP;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_DOWN) J |= VBA_DOWN;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_LEFT) J |= VBA_LEFT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_RIGHT) J |= VBA_RIGHT;
		if (wp->btns_h & (WPAD_CLASSIC_BUTTON_X | WPAD_CLASSIC_BUTTON_Y)) J |= VBA_BUTTON_B;
		if (wp->btns_h & (WPAD_CLASSIC_BUTTON_A | WPAD_CLASSIC_BUTTON_B)) J |= VBA_BUTTON_A;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS) J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS) J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZR) J |= VBA_BUTTON_B | VBA_RIGHT; // CAKTODO
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R) J |= VBA_BUTTON_START; // block
	}
#endif
	{
		u32 gc = PAD_ButtonsHeld(pad);
		// DPad moves
		if (gc & PAD_BUTTON_UP) J |= VBA_UP;
		if (gc & PAD_BUTTON_DOWN) J |= VBA_DOWN;
		if (gc & PAD_BUTTON_LEFT) J |= VBA_LEFT;
		if (gc & PAD_BUTTON_RIGHT) J |= VBA_RIGHT;
		if (gc & PAD_BUTTON_START) J |= VBA_BUTTON_START;
		if (gc & (PAD_BUTTON_B | PAD_BUTTON_Y)) J |= VBA_BUTTON_B;
		if (gc & (PAD_BUTTON_A | PAD_BUTTON_X)) J |= VBA_BUTTON_A;
		if (gc & PAD_TRIGGER_Z) J |= VBA_BUTTON_B | VBA_RIGHT; // CAKTODO
		if (gc & PAD_TRIGGER_R) J |= VBA_BUTTON_START; // block
	}

	return J;
}

u32 MKAInput(unsigned short pad)
{
	u32 J = StandardMovement(pad) | DecodeKeyboard(pad);
	u8 Health = 0;
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	// Punch
	if (wp->btns_h & WPAD_BUTTON_UP || wp->btns_h & WPAD_BUTTON_LEFT)
		J |= VBA_BUTTON_B;
	// Kick
	if (wp->btns_h & WPAD_BUTTON_DOWN || wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_A;
	// Block
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_BUTTON_R;
	// Run (supposed to be change styles)
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
		J |= VBA_BUTTON_L;
	// Throw
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_RIGHT; // CAKTODO check which way we are facing
	// Pause
	if (wp->btns_h & WPAD_BUTTON_PLUS || wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;
	// Special move
	if (wp->btns_h & WPAD_BUTTON_B)
	{
		// CAKTODO
	}
	if (wp->exp.type == WPAD_EXP_CLASSIC) {
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_UP) J |= VBA_UP;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_DOWN) J |= VBA_DOWN;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_LEFT) J |= VBA_LEFT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_RIGHT) J |= VBA_RIGHT;
		if (wp->btns_h & (WPAD_CLASSIC_BUTTON_X | WPAD_CLASSIC_BUTTON_Y)) J |= VBA_BUTTON_B;
		if (wp->btns_h & (WPAD_CLASSIC_BUTTON_A | WPAD_CLASSIC_BUTTON_B)) J |= VBA_BUTTON_A;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS) J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS) J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZR) J |= VBA_RIGHT; // CAKTODO
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R) J |= VBA_BUTTON_R; // block
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L) J |= VBA_BUTTON_L; // run
	}
#endif
	{
		u32 gc = PAD_ButtonsHeld(pad);
		// DPad moves
		if (gc & PAD_BUTTON_UP) J |= VBA_UP;
		if (gc & PAD_BUTTON_DOWN) J |= VBA_DOWN;
		if (gc & PAD_BUTTON_LEFT) J |= VBA_LEFT;
		if (gc & PAD_BUTTON_RIGHT) J |= VBA_RIGHT;
		if (gc & PAD_BUTTON_START) J |= VBA_BUTTON_START;
		if (gc & (PAD_BUTTON_B | PAD_BUTTON_Y)) J |= VBA_BUTTON_B;
		if (gc & (PAD_BUTTON_A | PAD_BUTTON_X)) J |= VBA_BUTTON_A;
		if (gc & PAD_TRIGGER_Z) J |= VBA_RIGHT; // CAKTODO
		if (gc & PAD_TRIGGER_R) J |= VBA_BUTTON_R; // block
		if (gc & PAD_TRIGGER_L) J |= VBA_BUTTON_L; // block
	}

	return J;
}

u32 MKTEInput(unsigned short pad)
{
	static u32 prevJ = 0, prevPrevJ = 0;
	u32 J = StandardMovement(pad) | DecodeKeyboard(pad);
	bool throwButton = false;
	u8 Health;
	u8 Side;
	if (RomIdCode & 0xFFFFFF == MKDA)
	{
		Health = CPUReadByte(0x3000760); // 731 or 760
		Side = CPUReadByte(0x3000747);
	} else {
		Health = CPUReadByte(0x3000761); // or 790
		Side = CPUReadByte(0x3000777);
	}
	u32 Forwards, Back;
	if (Side == 0) {
		Forwards = VBA_RIGHT;
		Back = VBA_LEFT;
	} else {
		Forwards = VBA_LEFT;
		Back = VBA_RIGHT;
	}

	// Rumble when they lose health!
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	// Punch
	if (wp->btns_h & WPAD_BUTTON_UP || wp->btns_h & WPAD_BUTTON_LEFT)
		J |= VBA_BUTTON_B;
	// Kick
	if (wp->btns_h & WPAD_BUTTON_DOWN || wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_A;
	// Block
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_BUTTON_R;
	// Change styles
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
		J |= VBA_BUTTON_L;
	// Throw
	if (wp->btns_h & WPAD_BUTTON_A) throwButton=true;
	// Pause
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;
	// Start
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Special move
	if (wp->btns_h & WPAD_BUTTON_B) {
		// CAKTODO
	}
	// Speed
	if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2) {
		J |= VBA_SPEED;
	}
	if (wp->exp.type == WPAD_EXP_CLASSIC) {
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_UP) J |= VBA_UP;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_DOWN) J |= VBA_DOWN;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_LEFT) J |= VBA_LEFT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_RIGHT) J |= VBA_RIGHT;
		if (wp->btns_h & (WPAD_CLASSIC_BUTTON_X | WPAD_CLASSIC_BUTTON_Y)) J |= VBA_BUTTON_B;
		if (wp->btns_h & (WPAD_CLASSIC_BUTTON_A | WPAD_CLASSIC_BUTTON_B)) J |= VBA_BUTTON_A;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS) J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS) J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZR) throwButton = true;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R) J |= VBA_BUTTON_R; // block
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L) J |= VBA_BUTTON_L; // run
	}
#endif
	{
		u32 gc = PAD_ButtonsHeld(pad);
		// DPad moves
		if (gc & PAD_BUTTON_UP) J |= VBA_UP;
		if (gc & PAD_BUTTON_DOWN) J |= VBA_DOWN;
		if (gc & PAD_BUTTON_LEFT) J |= VBA_LEFT;
		if (gc & PAD_BUTTON_RIGHT) J |= VBA_RIGHT;
		if (gc & PAD_BUTTON_START) J |= VBA_BUTTON_START;
		if (gc & (PAD_BUTTON_B | PAD_BUTTON_Y)) J |= VBA_BUTTON_B;
		if (gc & (PAD_BUTTON_A | PAD_BUTTON_X)) J |= VBA_BUTTON_A;
		if (gc & PAD_TRIGGER_Z) J |= throwButton = true;
		if (gc & PAD_TRIGGER_R) J |= VBA_BUTTON_R; // block
		if (gc & PAD_TRIGGER_L) J |= VBA_BUTTON_L; // change styles
	}
	if (throwButton)
	{
		if ((prevJ & Forwards && prevJ & VBA_BUTTON_A && prevJ & VBA_BUTTON_B) || ((prevPrevJ & Forwards) && !(prevJ & Forwards)))
			J |= Forwards | VBA_BUTTON_A | VBA_BUTTON_B; // R, R+1+2 = throw

		else if (prevJ & Forwards)
		{
			J &= ~Forwards;
			J &= ~VBA_BUTTON_A;
			J &= ~VBA_BUTTON_B;
		}
		else
			J |= Forwards;

	}

	if ((J & 48) == 48)
		J &= ~16;
	if ((J & 192) == 192)
		J &= ~128;
	prevPrevJ = prevJ;
	prevJ = J;

	return J;
}


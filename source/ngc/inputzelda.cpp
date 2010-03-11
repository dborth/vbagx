/****************************************************************************
 * Visual Boy Advance GX
 *
 * Carl Kenner Febuary 2009
 *
 * inputzelda.cpp
 *
 * Wii/Gamecube controls for Legend of Zelda games
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
#include "gui/gui.h"
#include "gameinput.h"
#include "vbasupport.h"
#include "gba/GBA.h"
#include "gba/bios.h"
#include "gba/GBAinline.h"

//#define ALLOWCHEAT

u8 ZeldaDxLeftPos = 2, ZeldaDxRightPos = 3, ZeldaDxDownPos = 4;
u8 ZeldaDxShieldPos = 5, ZeldaDxSwordPos = 5, ZeldaDxBraceletPos = 5;

void ZeldaSwap(u8 pos1, u8 pos2, u16 addr)
{
	u8 OldItem = gbReadMemory(addr + pos1);
	gbWriteMemory(addr + pos1, gbReadMemory(addr + pos2));
	gbWriteMemory(addr + pos2, OldItem);
}

u8 DrawnItemPos = 0xFF;

bool ZeldaDrawItem(u8 ItemNumber, u16 addr, int boxes)
{
	if (gbReadMemory(addr + 1) == ItemNumber)
		return true;
	DrawnItemPos = 0xFF;
	for (int i = 0; i < boxes; i++)
	{
		if (gbReadMemory(addr + i) == ItemNumber)
		{
			DrawnItemPos = i;
		}
	}
	if (DrawnItemPos == 0xFF)
		return false;
	if (DrawnItemPos != 1)
	{

		gbWriteMemory(addr + DrawnItemPos, gbReadMemory(addr + 1)); // put A item away
		gbWriteMemory(addr + 1, ItemNumber); // set A item to shield
	}
	return true;
}

bool ZeldaDxDrawBombs()
{
	if (gbReadMemory(0xDB00 + 1) == 2)
		return true;
	ZeldaDxShieldPos = 0xFF;
	for (int i = 0; i <= 11; i++)
	{
		if (gbReadMemory(0xDB00 + i) == 2)
		{
			ZeldaDxShieldPos = i;
		}
	}
	if (ZeldaDxShieldPos == 0xFF)
		return false;
	if (ZeldaDxShieldPos != 2)
	{

		gbWriteMemory(0xDB00 + ZeldaDxShieldPos, gbReadMemory(0xDB00 + 1)); // put A item away
		gbWriteMemory(0xDB00 + 1, 2); // set A item to bombs
	}
	return true;
}

bool ZeldaDxDrawSword()
{
	if (gbReadMemory(0xDB00 + 1) == 1)
		return true;
	ZeldaDxSwordPos = 0xFF;
	for (int i = 0; i <= 11; i++)
	{
		if (gbReadMemory(0xDB00 + i) == 1)
		{
			ZeldaDxSwordPos = i;
		}
	}
	if (ZeldaDxSwordPos == 0xFF)
		return false;
	if (ZeldaDxSwordPos != 1)
	{
		gbWriteMemory(0xDB00 + ZeldaDxSwordPos, gbReadMemory(0xDB00 + 1)); // put A item away
		gbWriteMemory(0xDB00 + 1, 1); // set A item to sword
	}
	return true;
}

void ZeldaDxSheathSword()
{
	if (ZeldaDxSwordPos == 0xFF || gbReadMemory(0xDB00 + 1) != 1)
		return;
	gbWriteMemory(0xDB00 + 1, gbReadMemory(0xDB00 + ZeldaDxSwordPos));
	gbWriteMemory(0xDB00 + ZeldaDxSwordPos, 1);
	ZeldaDxSwordPos = 0xFF;
}

void ZeldaDxCheat()
{
#ifdef ALLOWCHEAT
	gbWriteMemory(0xDB00 + 11, 0);
	for (int i = 0; i <= 10; i++)
	{
		gbWriteMemory(0xDB00 + i, i + 1);
	}
	gbWriteMemory(0xDB15, 5); // leaves
	gbWriteMemory(0xDB43, 3); // bracelet level
	gbWriteMemory(0xDB44, 3); // shield level
	gbWriteMemory(0xDB45, 0x30); // arrows
	gbWriteMemory(0xDB49, 7); // songs
	gbWriteMemory(0xDB4C, 0x30); // powder
	gbWriteMemory(0xDB4D, 0x30); // bombs
	gbWriteMemory(0xDB4E, 3); // sword level
	gbWriteMemory(0xDB5D, 0x9); // rupees
	gbWriteMemory(0xDB5E, 0x99); // rupees
	gbWriteMemory(0xDBD0, 1); // keys
#endif
}

void ZeldaAgesCheat()
{
#ifdef ALLOWCHEAT
	int j = 1;
	for (int i = 0; i <= 14; i++, j++)
	{
		switch(j) {
			case 2: case 0xb: case 0x10: case 0x18:
				j++;
				break;
			case 7: case 0x12:
				j+=3;
		}
		gbWriteMemory(0xC688 + i, j);
	}
	gbWriteMemory(0xC697, 0);
	gbWriteMemory(0xC698, 0);
	gbWriteMemory(0xC699, 0);
	gbWriteMemory(0xC6AF, 3); // shield
	gbWriteMemory(0xC6B2, 3); // sword
	gbWriteMemory(0xC6B6, 2); // long shot?
	gbWriteMemory(0xC6B7, 3); // harp's chosen song
	gbWriteMemory(0xC6B8, 2); // power glove
	for (j=0xC69F; j<=0xC6A2; j++) gbWriteMemory(j, 0xFF); // secondary items
	gbWriteMemory(0xC6A3, 0x0C); // mermaid suit
	for (j=0xC6A4; j<=0xC6A5; j++) gbWriteMemory(j, 0xFF); // secondary items, Brother emblem
	for (j=0xC6B9; j<=0xC6BD; j++) gbWriteMemory(j, 0x05); // seeds
	gbWriteMemory(0xC6BF, 0xFF); // essences
	gbWriteMemory(0xCF14, 0xEA); // GBA shop
#endif
}

void ZeldaSeasonsCheat()
{
#ifdef ALLOWCHEAT
	gbWriteMemory(0xC68F, 0);
	gbWriteMemory(0xC690, 0);
	gbWriteMemory(0xC691, 0);
	int j = 1;
	for (int i = 0; i <= 16; i++, j++)
	{
		switch(j) {
			case 2: case 4: case 9: case 0xb: case 0x14: case 0x18:
				j++;
				break;
			case 0xF: case 0x1a:
				j+=4;
		}
		gbWriteMemory(0xC680 + i, j);
	}
	for (j=0xC696; j<=0xC69C; j++) gbWriteMemory(j, 0xFF); // secondary items
	gbWriteMemory(0xC6A9, 3); // shield
	gbWriteMemory(0xC6AA, 9); // bombs
	gbWriteMemory(0xC6AC, 3); // sword
	gbWriteMemory(0xC6B0, 0xF); // rod of seasons
	gbWriteMemory(0xC6B1, 2); // boomerang
	gbWriteMemory(0xC6B2, 2); // ?
	gbWriteMemory(0xC6B3, 2); // sling shot
	gbWriteMemory(0xC6B4, 2); // feather/cape
	for (j=0xC6B5; j<=0xC6BA; j++) gbWriteMemory(j, 0x05); // seeds
	gbWriteMemory(0xC6BF, 0xFF); // essences
	gbWriteMemory(0xC6C6, 3); // ring box
	gbWriteMemory(0xCF14, 0xEA); // GBA shop
#endif
}

u32 LinksAwakeningInput(unsigned short pad) // aka Zelda DX
{
	u16 ItemsAddr = 0xDB00;
	static bool QuestScreen = false;
	static int StartCount = 0;
	static int SwordCount = 0;
	static bool BombArrows = false;
	static int DelayCount = 0;
	bool OnItemScreen = gbReadMemory(0xC16C) == 0x20; // 0x20 = items, 0x10 = normal

	// There is Zelda 1 & 2 for Wii VC wiimote but it doesn't make sense to use their controls,
	// so let user choose sideways wiimote controls.
	u32 J = StandardMovement(pad) | DecodeWiimote(pad);

	u8 CursorPos = gbReadMemory(0xC1B6) + 2;
	u8 SelItem = 0;
	if (CursorPos < 12)
		SelItem = gbReadMemory(ItemsAddr + CursorPos);

	// Rumble when they lose health!
	u8 Health = gbReadMemory(0xDB5A);
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	bool ActionButton=0, SwordButton=0, ShieldButton=0, PullButton=0,
	ItemsButton=0, QuestButton=0, MapButton=0, SpeedButton=0, CheatButton=0, MidnaButton=0,
#ifdef HW_RVL
	LeftItemButton=0, DownItemButton=0, RightItemButton=0,
#endif
	BItemButton=0, UseLeftItemButton=0, UseRightItemButton=0;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);
	// Nunchuk controls are based on Twilight Princess for the Wii
	if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		ActionButton = wp->btns_h & WPAD_BUTTON_A;
		ShieldButton = wp->btns_h & WPAD_NUNCHUK_BUTTON_Z;
		SpeedButton = wp->btns_h & WPAD_NUNCHUK_BUTTON_C;
		MidnaButton = wp->btns_h & WPAD_BUTTON_UP;
		LeftItemButton = wp->btns_d & WPAD_BUTTON_LEFT;
		DownItemButton = wp->btns_d & WPAD_BUTTON_DOWN;
		RightItemButton = wp->btns_d & WPAD_BUTTON_RIGHT;
		BItemButton = wp->btns_h & WPAD_BUTTON_B;
		ItemsButton = wp->btns_h & WPAD_BUTTON_MINUS;
		QuestButton = wp->btns_h & WPAD_BUTTON_PLUS;
		MapButton = wp->btns_h & WPAD_BUTTON_1;
		UseLeftItemButton = UseRightItemButton = false;
		CheatButton = wp->btns_h & WPAD_BUTTON_2;

		// Sword
		SwordButton = false;
		if (fabs(wp->gforce.x)> 1.5 && !OnItemScreen) {
			if (ZeldaDxDrawSword()) {
				if (SwordCount<3) SwordCount = 3;
			}
			QuestScreen = false;
		}
		// Spin attack
		if (fabs(wp->exp.nunchuk.gforce.x)> 0.6 && !OnItemScreen) {
			if (ZeldaDxDrawSword()) {
				if (SwordCount<60) SwordCount=60;
			}
			QuestScreen = false;
		}
		if (SwordCount>0) {
			if (SwordCount == 50)
				systemGameRumbleOnlyFor(50);
			if (!OnItemScreen)
				SwordButton = true;
			SwordCount--;
		}
	// Classic controller controls are loosely based on Ocarina of Time virtual console
	// but with R as the B button like in Twilight Princess
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		J |= StandardDPad(pad);
		s8 wm_sx = userInput[pad].WPAD_StickX(1); // CC right joystick
		s8 wm_sy = userInput[pad].WPAD_StickY(1); // CC right joystick
		static bool StickReady = true;
		ActionButton = wp->btns_h & WPAD_CLASSIC_BUTTON_A;
		SwordButton = wp->btns_h & WPAD_CLASSIC_BUTTON_B;
		ShieldButton = wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L || wp->exp.classic.ls_raw >= 0x10;
		LeftItemButton = wp->btns_d & WPAD_CLASSIC_BUTTON_ZR || (StickReady && wm_sx < -70);
		DownItemButton = wp->btns_d & WPAD_CLASSIC_BUTTON_Y || (StickReady && wm_sy < -70);
		RightItemButton = wp->btns_d & WPAD_CLASSIC_BUTTON_X || (StickReady && wm_sx > 70);
		MidnaButton = wm_sy > 70; // right stick up
		if (abs(wm_sx)>70 || abs(wm_sy)>70) StickReady = false;
		else if (abs(wm_sx)<50 && abs(wm_sy)<50) StickReady = true;
		BItemButton = wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R || wp->exp.classic.rs_raw >= 0x10;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		SpeedButton = wp->btns_h & WPAD_CLASSIC_BUTTON_ZL;
		// Must use Wiimote for these buttons
		CheatButton = wp->btns_d & WPAD_BUTTON_2;
	}
	CheatButton = CheatButton;
#endif
	// Gamecube controls are based on Twilight Princess for the Gamecube
	{
		u32 gc = PAD_ButtonsHeld(pad);
		s8 gc_px = PAD_SubStickX(pad);
		if (gc_px > 70) J |= VBA_SPEED;
		ActionButton = ActionButton || gc & PAD_BUTTON_A;
		PullButton = PullButton || gc & PAD_TRIGGER_R;
		SwordButton = SwordButton || gc & PAD_BUTTON_B;
		ShieldButton = ShieldButton || gc & PAD_TRIGGER_L;
		MidnaButton = MidnaButton || gc & PAD_TRIGGER_Z;
		UseLeftItemButton = UseLeftItemButton || gc & PAD_BUTTON_Y;
		UseRightItemButton = UseRightItemButton || gc & PAD_BUTTON_X;
		ItemsButton = ItemsButton || gc & PAD_BUTTON_UP;
		QuestButton = QuestButton || gc & PAD_BUTTON_START;
		MapButton = MapButton || gc & PAD_BUTTON_RIGHT;
		SpeedButton = SpeedButton || gc & PAD_BUTTON_DOWN;
		CheatButton = CheatButton || gc & PAD_BUTTON_LEFT;
	}

	// Action button, and put away sword
	if (ActionButton) {
		if (QuestScreen && OnItemScreen) {
			if (StartCount>=0) StartCount = -80;
		} else {
			if (OnItemScreen) systemGameRumble(5);
			else {
				// Unless they are trying to use 2 items at once, put away A item
				if (!BItemButton && !UseLeftItemButton && !UseRightItemButton) {
					if (!ZeldaDrawItem(0, ItemsAddr, 12)) // draw nothing if possible
						ZeldaDrawItem(4, ItemsAddr, 12); // or draw shield
				}
			}
			J |= VBA_BUTTON_A;
		}
	}
	// Sword button
	if (SwordButton) {
		if (ZeldaDxDrawSword())
			J |= VBA_BUTTON_A;
	}
	// Pull button (Gamecube R Trigger) automatically switches to bracelet
	if (PullButton) {
		if (ZeldaDrawItem(3, ItemsAddr, 12))
			J |= VBA_BUTTON_A;
	}
	// Shield and Z targetting
	if (ShieldButton && !OnItemScreen) {
		if (!SwordCount) {
			if (ZeldaDrawItem(4, ItemsAddr, 12))
				J |= VBA_BUTTON_A;
		}
		QuestScreen = false;
	}
	// Z Button Selects bomb arrows on or off
	if (ShieldButton && OnItemScreen) {
		if (SelItem==2 || SelItem==5) { // toggle bomb arrows
			BombArrows = !BombArrows;
			if (BombArrows) systemGameRumbleOnlyFor(16);
			else systemGameRumbleOnlyFor(4);
			if (SelItem==2 && BombArrows)
				J |= VBA_BUTTON_A;
		} else if (BombArrows) { // switch off bomb arrows
			BombArrows = false;
			systemGameRumbleOnlyFor(4);
			J |= VBA_BUTTON_A;
		}
		QuestScreen = false;
	}


	static bool BIsLeft = true;
	if (UseLeftItemButton)
	{
		if (!BIsLeft) {
			// Swap B with first inventory item
			ZeldaSwap(0, ZeldaDxLeftPos, ItemsAddr);
			BIsLeft = true;
		}
		if (OnItemScreen) {
			systemGameRumbleOnlyFor(5);
			J |= VBA_BUTTON_B;
		} else {
			u8 BButtonItem = gbReadMemory(ItemsAddr);
			if (BombArrows && (BButtonItem==5)) {
				if (ZeldaDrawItem(2, ItemsAddr, 12)) {
					J |= VBA_BUTTON_A;
					DelayCount++;
				}
			}
			else DelayCount = 10;
			if (DelayCount>1)
				J |= VBA_BUTTON_B;
		}
		QuestScreen = false;
	}
	if (UseRightItemButton)
	{
		if (BIsLeft) {
			// Swap B with first inventory item
			ZeldaSwap(0, ZeldaDxLeftPos, ItemsAddr);
			BIsLeft = false;
		}
		if (OnItemScreen) {
			systemGameRumbleOnlyFor(5);
			J |= VBA_BUTTON_B;
		} else {
			u8 BButtonItem = gbReadMemory(ItemsAddr);
			if (BombArrows && (BButtonItem==5)) {
				if (ZeldaDrawItem(2, ItemsAddr, 12)) {
					J |= VBA_BUTTON_A;
					DelayCount++;
				}
			}
			else DelayCount = 10;
			if (DelayCount>1)
				J |= VBA_BUTTON_B;
		}
		QuestScreen = false;
	}

#ifdef HW_RVL
	// Left Item
	if (LeftItemButton) {
		if (OnItemScreen) ZeldaSwap(ZeldaDxLeftPos, CursorPos, ItemsAddr);
		else ZeldaSwap(0, ZeldaDxLeftPos, ItemsAddr);
		systemGameRumbleOnlyFor(5);
		QuestScreen = false;
	}
	// Right Item
	if (RightItemButton) {
		if (OnItemScreen) ZeldaSwap(ZeldaDxRightPos, CursorPos, ItemsAddr);
		else ZeldaSwap(0, ZeldaDxRightPos, ItemsAddr);
		systemGameRumbleOnlyFor(5);
		QuestScreen = false;
	}
	// Down Item
	if (DownItemButton) {
		if (OnItemScreen) ZeldaSwap(ZeldaDxDownPos, CursorPos, ItemsAddr);
		else ZeldaSwap(0, ZeldaDxDownPos, ItemsAddr);
		systemGameRumbleOnlyFor(5);
		QuestScreen = false;
	}
	// B Item
	if (BItemButton) {
		if (QuestScreen && OnItemScreen) {
			if (StartCount>=0) StartCount = -80;
		} else {
			if (OnItemScreen) {
				/*if (BombArrows) {
				 if (SelItem==5) { // bow
				 } else {
				 BombArrows = false;
				 }
				 }*/
				systemGameRumble(5);
				DelayCount = 10;
			} else {
				u8 BButtonItem = gbReadMemory(ItemsAddr);
				if (BombArrows && (BButtonItem==5)) {
					if (ZeldaDrawItem(2, ItemsAddr, 12)) {
						J |= VBA_BUTTON_A;
						DelayCount++;
					}
				} else DelayCount = 10;
			}
			if (DelayCount>1)
				J |= VBA_BUTTON_B;
		}
	}
#endif
	if (!BItemButton && !UseLeftItemButton && !UseRightItemButton)
		DelayCount = 0;

	// Talk to Midna, er... I mean save the game
	if (MidnaButton) {
		J |= VBA_BUTTON_A | VBA_BUTTON_B | VBA_BUTTON_START | VBA_BUTTON_SELECT;
		systemGameRumbleOnlyFor(5);
		QuestScreen = false;
	}
	// Map
	if (MapButton) {
		QuestScreen = false;
		J |= VBA_BUTTON_SELECT;
	}
	// Items
	if (ItemsButton) {
		if (QuestScreen) QuestScreen = false;
		else J |= VBA_BUTTON_START;
	}
	// Quest Status
	if (QuestButton) {
		StartCount = 80;
		if (OnItemScreen) StartCount = -StartCount;
	}
	if (StartCount>0) {
		if (StartCount>75)
			J |= VBA_BUTTON_START;
		StartCount--;
		if (StartCount==0) QuestScreen = true;
	}
	else if (StartCount<0)
	{
		QuestScreen = false;
		if (StartCount>=-5)
			J |= VBA_BUTTON_START;
		StartCount++;
	}
	if (QuestScreen && OnItemScreen)
		J |= VBA_BUTTON_SELECT;

	// Cheat
	if (CheatButton) {
		ZeldaDxCheat();
		QuestScreen = false;
	}
	// Camera (fast forward)
	if (SpeedButton) {
		J |= VBA_SPEED;
		QuestScreen = false;
	}

	return J;
}

static u32 ZeldaOracleInput(bool Seasons, unsigned short pad) {
	u16 ItemsAddr;
	if (Seasons) ItemsAddr = 0xC680;
	else ItemsAddr = 0xC688;
	static u32 OldJ = 0;
	// There is Zelda 1 & 2 for Wii VC wiimote but it doesn't make sense to use their controls,
	// so let user choose sideways wiimote controls.
	u32 J = StandardMovement(pad) | DecodeWiimote(pad);

	// Rumble when they lose health!
	u8 Health;
	if (Seasons) Health = gbReadMemory(0xC6A2); // health in quarters... note C6A3 is max health
	else Health = gbReadMemory(0xC6AA); // health in quarters... note C6AB is max health
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	static int DesiredSubscreen = -1;
	int Subscreen = 0;
	switch (gbReadMemory(0xCBCB)) {
		case 0:
			Subscreen = 0;
			break;
		case 1:
			Subscreen = 1+gbReadMemory(0xCBCF);
			break;
		case 2:
			Subscreen = 4;
			break;
		case 3:
			Subscreen = 5;
			break;
	}

	bool ActionButton=0, SwordButton=0, ShieldButton=0, PullButton=0,
	ItemsButton=0, QuestButton=0, MapButton=0, SpeedButton=0, CheatButton=0, MidnaButton=0,
#ifdef HW_RVL
	LeftItemButton=0, DownItemButton=0, RightItemButton=0, BItemButton=0,
#endif
	UseLeftItemButton=0, UseRightItemButton=0;

	bool OnItemScreen = (Subscreen == 1);

#ifdef HW_RVL
	static int SwordCount = 0;
	WPADData * wp = WPAD_Data(pad);
	// Nunchuk controls are based on Twilight Princess for the Wii
	if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		ActionButton = wp->btns_h & WPAD_BUTTON_A;
		ShieldButton = wp->btns_h & WPAD_NUNCHUK_BUTTON_Z;
		SpeedButton = wp->btns_h & WPAD_NUNCHUK_BUTTON_C;
		MidnaButton = wp->btns_d & WPAD_BUTTON_UP;
		LeftItemButton = wp->btns_d & WPAD_BUTTON_LEFT;
		DownItemButton = wp->btns_d & WPAD_BUTTON_DOWN;
		RightItemButton = wp->btns_d & WPAD_BUTTON_RIGHT;
		BItemButton = wp->btns_h & WPAD_BUTTON_B;
		ItemsButton = wp->btns_d & WPAD_BUTTON_MINUS;
		QuestButton = wp->btns_d & WPAD_BUTTON_PLUS;
		MapButton = wp->btns_d & WPAD_BUTTON_1;
		CheatButton = wp->btns_d & WPAD_BUTTON_2;
		// Sword
		SwordButton = false;
		if (fabs(wp->gforce.x)> 1.5 && !OnItemScreen)
		{
			if (SwordCount<3) SwordCount = 3;
		}
		// Spin attack
		if (fabs(wp->exp.nunchuk.gforce.x)> 0.6 && !OnItemScreen)
		{
			if (SwordCount<60) SwordCount=60;
		}
		if (SwordCount>0)
		{
			if (SwordCount == 50)
				systemGameRumbleOnlyFor(50);
			if (!OnItemScreen)
				SwordButton = true;
			SwordCount--;
		}
	// Classic controller controls are loosely based on Ocarina of Time virtual console
	// but with R as the B button like in Twilight Princess
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		J |= StandardDPad(pad);
		s8 wm_sx = userInput[pad].WPAD_StickX(1); // CC right joystick
		s8 wm_sy = userInput[pad].WPAD_StickY(1); // CC right joystick
		static bool StickReady = true;
		ActionButton = wp->btns_h & WPAD_CLASSIC_BUTTON_A;
		SwordButton = wp->btns_h & WPAD_CLASSIC_BUTTON_B;
		ShieldButton = wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L || wp->exp.classic.ls_raw >= 0x10;
		LeftItemButton = wp->btns_d & WPAD_CLASSIC_BUTTON_ZR || (StickReady && wm_sx < -70);
		DownItemButton = wp->btns_d & WPAD_CLASSIC_BUTTON_Y || (StickReady && wm_sy < -70);
		RightItemButton = wp->btns_d & WPAD_CLASSIC_BUTTON_X || (StickReady && wm_sx > 70);
		if (abs(wm_sx)>70 || abs(wm_sy)>70) StickReady = false;
		else if (abs(wm_sx)<50 && abs(wm_sy)<50) StickReady = true;
		BItemButton = wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R || wp->exp.classic.rs_raw >= 0x10;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		SpeedButton = wp->btns_h & WPAD_CLASSIC_BUTTON_ZL;
		// Must use Wiimote for these buttons
		CheatButton = wp->btns_d & WPAD_BUTTON_2;
	}
	CheatButton = CheatButton;
#endif
	// Gamecube controls are based on Twilight Princess for the Gamecube
	{
		u32 gc = PAD_ButtonsHeld(pad);
		u32 pressed = PAD_ButtonsDown(pad);
		s8 gc_px = PAD_SubStickX(pad);
		if (gc_px > 70) J |= VBA_SPEED;
		ActionButton = ActionButton || gc & PAD_BUTTON_A;
		PullButton = PullButton || gc & PAD_TRIGGER_R;
		SwordButton = SwordButton || gc & PAD_BUTTON_B;
		ShieldButton = ShieldButton || gc & PAD_TRIGGER_L;
		MidnaButton = MidnaButton || pressed & PAD_TRIGGER_Z;
		UseLeftItemButton = UseLeftItemButton || gc & PAD_BUTTON_Y;
		UseRightItemButton = UseRightItemButton || gc & PAD_BUTTON_X;
		ItemsButton = ItemsButton || pressed & PAD_BUTTON_UP;
		QuestButton = QuestButton || pressed & PAD_BUTTON_START;
		MapButton = MapButton || pressed & PAD_BUTTON_RIGHT;
		SpeedButton = SpeedButton || gc & PAD_BUTTON_DOWN;
		CheatButton = CheatButton || gc & PAD_BUTTON_LEFT;
	}

	static int OldDesiredSubscreen = -1, DelayCount = 0;
	OldDesiredSubscreen = DesiredSubscreen;
	// Items
	if (ItemsButton) {
		if (Subscreen == 1) DesiredSubscreen = 0;
		else DesiredSubscreen = 1;
		DelayCount = 1;
	}
	// Talk to Midna, er... I mean go to secondary items screen
	if (MidnaButton) {
		if (Subscreen == 2) DesiredSubscreen = 0;
		else DesiredSubscreen = 2;
		DelayCount = 1;
	}
	// Quest Status
	if (QuestButton) {
		if (Subscreen == 3) DesiredSubscreen = 0;
		else DesiredSubscreen = 3;
		DelayCount = 1;
	}
	// Map
	if (MapButton) {
		if (Subscreen == 4) DesiredSubscreen = 0;
		else DesiredSubscreen = 4;
		DelayCount = 1;
	}

	// after using sword, need to release A button before using shield
	static int SheathCount = 0;
	if (OldJ & VBA_BUTTON_A) {
		if (gbReadMemory(ItemsAddr+1)==5)
			SheathCount = 15;
	}

#ifdef HW_RVL
	u8 CursorPos = gbReadMemory(0xCBD0)+2;

	// Can't swap items if using two handed sword unless on item screen
	if (OnItemScreen || (gbReadMemory(ItemsAddr+0)!=0x0C && gbReadMemory(ItemsAddr+1)!=0x0C)) {
		// Left Item
		if (LeftItemButton)
		{
			if (OnItemScreen) ZeldaSwap(2, CursorPos, ItemsAddr);
			else ZeldaSwap(0, ZeldaDxLeftPos, ItemsAddr);
			systemGameRumbleOnlyFor(5);
		}
		// Right Item
		if (RightItemButton)
		{
			if (OnItemScreen) ZeldaSwap(4, CursorPos, ItemsAddr);
			else ZeldaSwap(0, ZeldaDxRightPos, ItemsAddr);
			systemGameRumbleOnlyFor(5);
		}
		// Down Item
		if (DownItemButton)
		{
			if (OnItemScreen) ZeldaSwap(3, CursorPos, ItemsAddr);
			else ZeldaSwap(0, ZeldaDxDownPos, ItemsAddr);
			systemGameRumbleOnlyFor(5);
		}
	}
	// B Item
	if (BItemButton)
	{
		if (OnItemScreen) systemGameRumbleOnlyFor(5);
		J |= VBA_BUTTON_B;
	}
#endif

	static bool BIsLeft = true;
	if (UseLeftItemButton)
	{
		if (!BIsLeft) {
			// Fix two-handed sword before swap
			if (gbReadMemory(ItemsAddr+0)==0x0C && gbReadMemory(ItemsAddr+1)==0x0C && !OnItemScreen) {
			} else {
				bool DrawingTwoHanded = (gbReadMemory(ItemsAddr+2)==0x0C);
				if (DrawingTwoHanded)
					DrawingTwoHanded = ZeldaDrawItem(0, ItemsAddr, 18); // put A item away (by drawing emptiness)
				// Swap B with first inventory item
				ZeldaSwap(0, ZeldaDxLeftPos, ItemsAddr);
				if (DrawingTwoHanded)
					gbWriteMemory(ItemsAddr+1, 0x0C);
				BIsLeft = true;
			}
		}
		if (OnItemScreen) systemGameRumbleOnlyFor(5);
		J |= VBA_BUTTON_B;
	}
	if (UseRightItemButton)
	{
		if (BIsLeft) {
			// Fix two-handed sword before swap
			if (gbReadMemory(ItemsAddr+0)==0x0C && gbReadMemory(ItemsAddr+1)==0x0C && !OnItemScreen) {
			} else {
				bool DrawingTwoHanded = (gbReadMemory(ItemsAddr+2)==0x0C);
				if (DrawingTwoHanded)
					DrawingTwoHanded = ZeldaDrawItem(0, ItemsAddr, 18); // put A item away (by drawing emptiness)
				// Swap B with first inventory item
				ZeldaSwap(0, ZeldaDxLeftPos, ItemsAddr);
				if (DrawingTwoHanded)
					gbWriteMemory(ItemsAddr+1, 0x0C);
				BIsLeft = false;
			}
		}
		if (OnItemScreen) systemGameRumbleOnlyFor(5);
		J |= VBA_BUTTON_B;
	}

	// Action
	if (ActionButton) {
		if (!OnItemScreen) {
			// If not using 2-handed sword then switch to holding nothing or shield
			if (gbReadMemory(ItemsAddr+1)!=0x0C && !ZeldaDrawItem(0, ItemsAddr, 18))// draw nothing or shield
				ZeldaDrawItem(1, ItemsAddr, 18);
		}
		J |= VBA_BUTTON_A;
	}
	// Z-Targetting
	if (ShieldButton && !SwordButton) {
		// Fix two-handed sword before swap
		if (gbReadMemory(ItemsAddr+0)==0x0C && gbReadMemory(ItemsAddr+1)==0x0C)
			gbWriteMemory(ItemsAddr+0, 0);
		if (SheathCount>0) {// was using sword before
			ZeldaDrawItem(1, ItemsAddr, 18);
			SheathCount--;
		}
		else if (ZeldaDrawItem(1, ItemsAddr, 18))
			J |= VBA_BUTTON_A;
	}
	// Pulling
	if (PullButton) {
		// Fix two-handed sword before swap
		if (gbReadMemory(ItemsAddr+0)==0x0C && gbReadMemory(ItemsAddr+1)==0x0C)
			gbWriteMemory(ItemsAddr+0, 0);
		if (ZeldaDrawItem(0x16, ItemsAddr, 18))
			J |= VBA_BUTTON_A;
	}
	// Sword
	if (SwordButton) {
		if (ShieldButton && gbReadMemory(ItemsAddr+1)==1) // was using shield before
			ZeldaDrawItem(5, ItemsAddr, 18);
		else if (gbReadMemory(ItemsAddr)==0x0C || gbReadMemory(ItemsAddr+1)==0x0C || ZeldaDrawItem(5, ItemsAddr, 18))
			J |= VBA_BUTTON_A;
	}
	// Camera (fast forward)
	if (SpeedButton)
		J |= VBA_SPEED;
	// Cheat
	if (CheatButton) {
		if (Seasons) ZeldaSeasonsCheat();
		else ZeldaAgesCheat();
	}

	if (DesiredSubscreen == Subscreen)
		DesiredSubscreen = -1;
	static int OldSubscreen = 0;
	if (Subscreen != OldSubscreen) {
		if (Subscreen==1) DelayCount = 80; // wait for items screen to fade in
		else DelayCount = 20; // wait to swap subscreens
	}
	OldSubscreen = Subscreen;

	if (DelayCount > 0) {
		// do nothing
		DelayCount--;
	} else if (DesiredSubscreen == 0) { // game
		switch(Subscreen) {
			case 1: case 2: case 3: case 5: default:
				J |= VBA_BUTTON_START;
				break;
			case 4: // map
				J |= VBA_BUTTON_SELECT;
		}
	} else if (DesiredSubscreen == 1) { // items
		switch(Subscreen) {
			case 0: case 5: default: // game or save
				J |= VBA_BUTTON_START;
				break;
			case 2: case 3: case 4:
				J |= VBA_BUTTON_SELECT;
		}
	} else if (DesiredSubscreen == 2) { // secondary items
		switch(Subscreen) {
			case 0: case 5: default: // game or save
				J |= VBA_BUTTON_START;
				break;
			case 1: case 3: case 4:
				J |= VBA_BUTTON_SELECT;
		}
	} else if (DesiredSubscreen == 3) { // quest status
		switch(Subscreen) {
			case 0: case 5: default: // game or save
				J |= VBA_BUTTON_START;
				break;
			case 1: case 2: case 4:
				J |= VBA_BUTTON_SELECT;
		}
	} else if (DesiredSubscreen == 4) { // map
		switch(Subscreen) {
			case 0: // game
				J |= VBA_BUTTON_SELECT;
				break;
			case 1: case 2: case 3: case 5: default:
				J |= VBA_BUTTON_START;
		}
	} else if (DesiredSubscreen == 5) { // save
		switch(Subscreen) {
			case 0: default: // game
				J |= VBA_BUTTON_START;
				break;
			case 1: case 2: case 4: // other
				J |= VBA_BUTTON_SELECT;
				break;
			case 3: // quest status
				J |= VBA_RIGHT;
				// give up, this is as close as I can be bothered to get
				DesiredSubscreen = -1;
		}
	}
	OldJ = J;
	return J;
}

u32 OracleOfAgesInput(unsigned short pad)
{
	return ZeldaOracleInput(false, pad);
}

u32 OracleOfSeasonsInput(unsigned short pad)
{
	return ZeldaOracleInput(true, pad);
}

u32 MinishCapInput(unsigned short pad)
{
	u32 J = StandardMovement(pad) | DecodeWiimote(pad);

	// Rumble when they lose health!
	u8 Health = CPUReadByte(0x2002aea);
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	static u8 SubscreenWanted = 0xFF;
	static bool waiting = false;
	u8 Subscreen = CPUReadByte(0x200008C);
	if (Subscreen == 0x64)
		Subscreen = 0; // Boss battle (balloon)

	u8 GameStart = CPUReadByte(0x2000086);

	u8 SelBox = CPUReadByte(0x2000083);


	u8 LoadMenu = CPUReadByte(0x200AF57);
	static u8 AButtonItem = 0;
	static u8 BButtonItem = 0;
	if (Subscreen == 0x2c) {
		AButtonItem = CPUReadByte(0x200af3c);
		BButtonItem = CPUReadByte(0x200af5c);
	} else if (Subscreen == 0) {
		AButtonItem = CPUReadByte(0x200af5c);
		BButtonItem = CPUReadByte(0x200af7c);
	}
	static int frame = 0;
	u32 SwordButtonNumber = 0;
	u32 ZTargetButton = 0;
	if (AButtonItem <= 6 && AButtonItem >= 1)
		SwordButtonNumber = VBA_BUTTON_A;
	else if (BButtonItem <= 6 && BButtonItem >= 1)
		SwordButtonNumber = VBA_BUTTON_B;
	else
		SwordButtonNumber = VBA_BUTTON_START;
	// Shield
	if (BButtonItem >= 0xD && BButtonItem <= 0xE)
		ZTargetButton = VBA_BUTTON_B;
	else if (AButtonItem >= 0xD && AButtonItem <= 0xE)
		ZTargetButton = VBA_BUTTON_A;
	// Gust Jar also does Z targetting
	else if (BButtonItem == 0x11)
		ZTargetButton = VBA_BUTTON_B;
	else if (AButtonItem == 0x11)
		ZTargetButton = VBA_BUTTON_A;
	else
		ZTargetButton = 0;

	bool ActionButton=0, SwordButton=0, ShieldButton=0, PullButton=0,
	ItemsButton=0, QuestButton=0, MapButton=0, SpeedButton=0, CheatButton=0, MidnaButton=0,
	LeftItemButton=0, DownItemButton=0, RightItemButton=0,
	BItemButton=0, UseLeftItemButton=0, UseRightItemButton=0;


#ifdef HW_RVL
	bool OnItemScreen = (Subscreen==0x2c);
	u8 RButtonAction = CPUReadByte(0x200af32);
	WPADData * wp = WPAD_Data(pad);
	// Wii Pointer selection on item screen
	int cx, cy, SelRow, SelCol, CursorRow = 0xFF, CursorCol = 0xFF;
	static int OldCursorRow = 0xFF;
	static int OldCursorCol = 0xFF;
	CursorVisible = ((Subscreen != 0 && Subscreen != 0x64) || LoadMenu == 2);
	if (CursorVisible) {
		cx = (CursorX * 240) / 640;
		cy = (CursorY * 160) / 480;
	} else {
		cx = -1;
		cy = -1;
	}
	if (Subscreen == 0x2c) {
		SelRow = SelBox / 4;
		SelCol = SelBox % 4;
		if (SelBox == 16) {
			SelRow = 3;
			SelCol = 4;
		}
		else if (SelRow < 3 && SelCol >= 2)
			SelCol++;
		if (cy >= 35 && cy <= 55)
			CursorRow = 0;
		else if (cy >= 59 && cy <= 79)
			CursorRow = 1;
		else if (cy >= 83 && cy <= 103)
			CursorRow = 2;
		else if (cy >= 107 && cy <= 127)
			CursorRow = 3;
		else
			CursorRow = 0xFF;
		if (CursorRow < 3)
		{
			if (cx >= 52 && cx <= 79)
				CursorCol = 0;
			else if (cx >= 88 && cx <= 116)
				CursorCol = 1;
			else if (cx >= 124 && cx <= 152)
				CursorCol = 3;
			else if (cx >= 160 && cx <= 188)
				CursorCol = 4;
			else
				CursorCol = 0xFF;
		}
		else
		{
			if (cx >= 157 && cx <= 204)
				CursorCol = 4;
			else if (cx >= 52 && cx < 152)
				CursorCol = (cx - 52) / 25;
			else
				CursorCol = 0xFF;
		}
		if (CursorCol != 0xFF && CursorRow != 0xFF && (CursorCol != SelCol
				|| CursorRow != SelRow))
		{
			if (CursorCol > SelCol)
				J |= VBA_RIGHT;
			else if (CursorCol < SelCol)
				J |= VBA_LEFT;
			else if (CursorRow > SelRow)
				J |= VBA_DOWN;
			else if (CursorRow < SelRow)
				J |= VBA_UP;
		}
	}
	else if (Subscreen == 0x38)
	{
		switch (SelBox)
		{
			case 0:
			case 1:
			case 10:
			case 9:
			case 11:
				SelRow = 0;
				break;
			case 12:
				SelRow = 1;
				break;
			case 2:
			case 3:
			case 13:
			case 14:
			case 15:
				SelRow = 2;
				break;
			case 6:
			case 7:
			case 8:
			case 4:
			case 5:
				SelRow = 3;
				break;
			default:
				SelRow = 0xFF;
				break;
		}
		switch (SelBox)
		{
			case 0:
			case 2:
			case 6:
				SelCol = 0;
				break;
			case 7:
				SelCol = 1;
				break;
			case 1:
			case 3:
			case 8:
				SelCol = 2;
				break;
			case 10:
			case 13:
			case 4:
				SelCol = 3;
				break;
			case 9:
			case 12:
			case 14:
				SelCol = 4;
				break;
			case 11:
			case 15:
			case 5:
				SelCol = 5;
				break;
			default:
				SelCol = 0xFF;
				break;
		}
		if (cy >= 109 && cy <= 130) {
			CursorRow = 3;
			if (cx >= 36 && cx <= 113)
				CursorCol = (cx - 36) / 26;
			else if (cx >= 118 && cx <= 165)
				CursorCol = 3;
			else if (cx >= 168 && cx <= 215)
				CursorCol = 5;
			else
				CursorCol = 0xFF;
		} else {
			if (cx >= 35 && cx <= 70) {
				CursorCol = 0;
				if (cy >= 40 && cy <= 73)
					CursorRow = 0;
				else if (cy >= 79 && cy <= 104)
					CursorRow = 2;
				else
					CursorRow = 0xFF;
			} else if (cx >= 78 && cx <= 113) {
				CursorCol = 2;
				if (cy >= 40 && cy <= 73)
					CursorRow = 0;
				else if (cy >= 79 && cy <= 104)
					CursorRow = 2;
				else
					CursorRow = 0xFF;
			} else {
				if (cx >= 125 && cx <= 148) {
					CursorCol = 3;
					if (cy >= 44 && cy <= 65)
						CursorRow = 0;
					else if (cy >= 83 && cy <= 104)
						CursorRow = 2;
					else
						CursorRow = 0xFF;
				} else if (cx >= 151 && cx <= 172) {
					CursorCol = 4;
					if (cy >= 31 && cy <= 53)
						CursorRow = 0;
					else if (cy >= 57 && cy <= 78)
						CursorRow = 1;
					else if (cy >= 83 && cy <= 104)
						CursorRow = 2;
					else
						CursorRow = 0xFF;
				} else if (cx >= 175 && cx <= 198) {
					CursorCol = 5;
					if (cy >= 44 && cy <= 65)
						CursorRow = 0;
					else if (cy >= 83 && cy <= 104)
						CursorRow = 2;
					else
						CursorRow = 0xFF;
				} else {
					CursorCol = 0xFF;
					CursorRow = 0xFF;
				}
			}
		}

		if (CursorCol != 0xFF && CursorRow != 0xFF && (CursorCol != SelCol
				|| CursorRow != SelRow)) {
			if (CursorCol > SelCol)
				J |= VBA_RIGHT;
			else if (CursorCol < SelCol)
				J |= VBA_LEFT;
			else if (CursorRow > SelRow)
				J |= VBA_DOWN;
			else if (CursorRow < SelRow)
				J |= VBA_UP;
		}
	}
	if ((CursorRow == 0xFF || CursorCol == 0xFF) && (OldCursorRow == 0xFF || OldCursorCol == 0xFF)) {
		// no change, still not pointing at anything
	} else if (CursorVisible && (CursorRow != OldCursorRow || CursorCol != OldCursorCol)) {
		// Cursor changed buttons, so rumble
		//systemGameRumble(5);
	}
	OldCursorRow = CursorRow;
	OldCursorCol = CursorCol;

	static int SwordCount = 0;
	// Nunchuk controls are based on Twilight Princess for the Wii
	if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		ActionButton = wp->btns_h & WPAD_BUTTON_A;
		ShieldButton = wp->btns_h & WPAD_NUNCHUK_BUTTON_Z;
		SpeedButton = wp->btns_h & WPAD_NUNCHUK_BUTTON_C;
		MidnaButton = wp->btns_h & WPAD_BUTTON_UP;
		LeftItemButton = wp->btns_h & WPAD_BUTTON_LEFT;
		DownItemButton = wp->btns_h & WPAD_BUTTON_DOWN;
		RightItemButton = wp->btns_h & WPAD_BUTTON_RIGHT;
		BItemButton = wp->btns_h & WPAD_BUTTON_B;
		ItemsButton = wp->btns_d & WPAD_BUTTON_MINUS;
		QuestButton = wp->btns_d & WPAD_BUTTON_PLUS;
		MapButton = wp->btns_d & WPAD_BUTTON_1;
		CheatButton = wp->btns_h & WPAD_BUTTON_2;
		// Sword
		SwordButton = false;
		if (fabs(wp->gforce.x)> 1.5 && !OnItemScreen) {
			if (SwordCount<3) SwordCount = 3;
		}
		// Throw gesture
		if (RButtonAction==0x03) {
			if (fabs(wp->exp.nunchuk.gforce.y)> 0.6) {
				J |= VBA_BUTTON_R;
				systemGameRumble(5);
			}
		// Spin attack
		} else if (fabs(wp->exp.nunchuk.gforce.x)> 0.6 && !OnItemScreen) {
			if (SwordCount<60) SwordCount=60;
		}
		if (SwordCount>0) {
			if (SwordCount == 50)
				systemGameRumbleOnlyFor(50);
			if (!OnItemScreen)
				SwordButton = true;
			SwordCount--;
		}
	// Classic controller controls are loosely based on Ocarina of Time virtual console
	// but with R as the B button like in Twilight Princess
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		J |= StandardDPad(pad);
		s8 wm_sx = userInput[pad].WPAD_StickX(1); // CC right joystick
		s8 wm_sy = userInput[pad].WPAD_StickY(1); // CC right joystick
		static bool StickReady = true;
		ActionButton = wp->btns_h & WPAD_CLASSIC_BUTTON_A;
		SwordButton = wp->btns_h & WPAD_CLASSIC_BUTTON_B;
		ShieldButton = wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L || wp->exp.classic.ls_raw >= 0x10;
		if (abs(wm_sx)>80 || abs(wm_sy)>80) StickReady = true;
		LeftItemButton = wp->btns_h & WPAD_CLASSIC_BUTTON_ZR || (StickReady && wm_sx < -70);
		DownItemButton = wp->btns_h & WPAD_CLASSIC_BUTTON_Y || (StickReady && wm_sy < -70);
		RightItemButton = wp->btns_h & WPAD_CLASSIC_BUTTON_X || (StickReady && wm_sx > 70);
		MidnaButton = wm_sy > 70;
		if (abs(wm_sx)<70 && abs(wm_sy)<70) StickReady = false;
		BItemButton = wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R || wp->exp.classic.rs_raw >= 0x10;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		SpeedButton = wp->btns_h & WPAD_CLASSIC_BUTTON_ZL;
		// Must use Wiimote for these buttons
		CheatButton = wp->btns_h & WPAD_BUTTON_2;
	}
	CheatButton = CheatButton;
#endif
	// Gamecube controls are based on Twilight Princess for the Gamecube
	{
		u32 gc = PAD_ButtonsHeld(pad);
		u32 pressed = PAD_ButtonsDown(pad);
		s8 gc_px = PAD_SubStickX(pad);
		if (gc_px > 70) J |= VBA_SPEED;
		ActionButton = ActionButton || gc & PAD_BUTTON_A;
		PullButton = PullButton || gc & PAD_TRIGGER_R;
		SwordButton = SwordButton || gc & PAD_BUTTON_B;
		ShieldButton = ShieldButton || gc & PAD_TRIGGER_L;
		MidnaButton = MidnaButton || pressed & PAD_TRIGGER_Z;
		UseLeftItemButton = UseLeftItemButton || gc & PAD_BUTTON_Y;
		UseRightItemButton = UseRightItemButton || gc & PAD_BUTTON_X;
		ItemsButton = ItemsButton || pressed & PAD_BUTTON_UP;
		QuestButton = QuestButton || pressed & PAD_BUTTON_START;
		MapButton = MapButton || pressed & PAD_BUTTON_RIGHT;
		SpeedButton = SpeedButton || gc & PAD_BUTTON_DOWN;
		LeftItemButton = CheatButton || gc & PAD_BUTTON_LEFT;
	}

	if (ItemsButton) { // items
		if (Subscreen == 0x2c) SubscreenWanted = 0;
		else SubscreenWanted = 0x2c;
		waiting = false;
	} else if (MapButton) { // map
		if (Subscreen == 0x0c || Subscreen==0x70) SubscreenWanted = 0;
		else SubscreenWanted = 0x0c;
		waiting = false;
	} else if (QuestButton) { // quest
		if (Subscreen == 0x38) SubscreenWanted = 0;
		else SubscreenWanted = 0x38;
		waiting = false;
	} else if (ActionButton && Subscreen == 0x2c && SelBox!=16) {
		SubscreenWanted = 0;
		waiting = false;
	} else if (BItemButton && Subscreen == 0x38) {
		SubscreenWanted = 0;
		waiting = false;
	}
	if (GameStart==2 || GameStart==3) {
		SubscreenWanted = 0;
		waiting = false;
		if (QuestButton || (ActionButton && BButtonItem))
			J |= VBA_BUTTON_START;
		return J;
	} else if (LoadMenu==2) {
		if (MidnaButton)
			J |= VBA_UP;
		if (DownItemButton)
			J |= VBA_DOWN;
		if (LeftItemButton)
			J |= VBA_LEFT;
		if (RightItemButton)
			J |= VBA_RIGHT;
		if (ActionButton)
			J |= VBA_BUTTON_A;
		if (BButtonItem)
			J |= VBA_BUTTON_B;

		if (QuestButton)
			J |= VBA_BUTTON_R;
		if (ItemsButton)
			J |= VBA_BUTTON_L;
		if (MapButton)
			J |= VBA_BUTTON_SELECT;
		if (CheatButton)
			J |= VBA_BUTTON_START;
		if (SpeedButton)
			J |= VBA_BUTTON_SELECT;
		if (ShieldButton)
			J |= VBA_BUTTON_START;
		return J;
	}

	//0c = map
	//70 = dungeon map
	//38 = quest
	//2c = items
	//89 = KINSTONE PIECES
	//f0 = SWORD TECHNIQUES
	//00 = none or changing or save
	//64 = Boss battle (balloon thing)
	//FF = don't care!
	//;
	if (Subscreen == 0x70 && SubscreenWanted == 0x0c) {
		SubscreenWanted = 0xFF;
		waiting = false;
	}

	if (Subscreen != SubscreenWanted && SubscreenWanted != 0xFF) {
		frame++;
		if (frame % 2 == 0)
			switch (Subscreen) {
				case 0:
					frame = 0;
					if (!waiting) {
						waiting = true;
						//waittime =
						J |= VBA_BUTTON_START;
					}
					break;
				case 0x0c: // map
				case 0x70:
					if (SubscreenWanted == 0x38)
						J |= VBA_BUTTON_L; // quest
					else if (SubscreenWanted == 0x2c)
						J |= VBA_BUTTON_R; // items
					else
						J |= VBA_BUTTON_START;
					waiting = false;
					break;
				case 0x2c: // items
					if (SubscreenWanted == 0x0c || SubscreenWanted == 0x70)
						J |= VBA_BUTTON_L; // map
					else if (SubscreenWanted == 0x38)
						J |= VBA_BUTTON_R; // quest
					else
						J |= VBA_BUTTON_START;
					waiting = false;
					break;
				case 0x38: // quest
					if (SubscreenWanted == 0x2c)
						J |= VBA_BUTTON_L; // map
					else if (SubscreenWanted == 0x0c || SubscreenWanted == 0x70)
						J |= VBA_BUTTON_R; // quest
					else
						J |= VBA_BUTTON_START;
					// CAKTODO: kinstone and sword techniques
					waiting = false;
					break;
				case 0x89: // kinstone
				case 0xF0: // sword techniques
					J |= VBA_BUTTON_B; // cancel
					waiting = false;
					break;

			}
	} else {
		SubscreenWanted = 0xFF;
		waiting = false;
	}

	/*** Report wp->btns_h buttons (gamepads) ***/

	// Talk to Midna, er... I mean Ezlo
	if (MidnaButton)
		J |= VBA_BUTTON_SELECT;

	// Save button
	if ((Subscreen==0x2c && SelBox==16) || (Subscreen==0x38 && SelBox==5)) {
		if (ActionButton) {
			J |= VBA_BUTTON_A;
			systemGameRumble(12);
		}
	} else if (Subscreen==0x2c) {
	} else if (Subscreen==0x38 && SelBox==4) { // Sleep button returns to menu instead of sleep
		if (ActionButton) {
			ConfigRequested = 1;
			return 0;
		}
	} else if (Subscreen==0x38) { // Action button counts as A Button on quest status screen
		// Action
		if (ActionButton) {
			J |= VBA_BUTTON_A;
			//systemGameRumble(4);
		}
	} else {
		// Action
		if (ActionButton)
			J |= VBA_BUTTON_R;
	}
	// Right Item
	if (RightItemButton || UseRightItemButton) {
		J |= VBA_BUTTON_A;
		if (Subscreen==0x2c) {
			systemGameRumble(10);
		}
	}
	// Down Item
	if (Subscreen==0x38) {
	} else {
		if (BItemButton) {
			J |= VBA_BUTTON_B;
			if (Subscreen==0x2c) {
				systemGameRumble(10);
			}
		}
	}
	if (DownItemButton || UseLeftItemButton) {
		J |= VBA_BUTTON_B;
		if (Subscreen==0x2c) {
			systemGameRumble(10);
		}
	}
	// Kinstone (doesn't work in items screen)
	if (LeftItemButton && Subscreen != 0x2c)
		J |= VBA_BUTTON_L;
	// Menu
	if (CheatButton)
		J |= VBA_BUTTON_START;
	// Sword
	if (Subscreen == 0 && SwordButton)
		J |= SwordButtonNumber;
	// Z-Targetting
	if (Subscreen==0 && ShieldButton)
		J |= ZTargetButton;
	// Camera (use as extra way of pressing A without using DPad)
	if (SpeedButton)
		J |= VBA_BUTTON_A;
	// Pull (I was going to check RButtonAction, but the in-game text always tells you to
	// use R as the action button, so let's just make Gamecube R = Gameboy R)
	if (PullButton)
		J |= VBA_BUTTON_R;

	return J;
}

u32 ALinkToThePastInput(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u8 Health = 0;
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	// Talk to Midna, er... I mean ... nobody
	if (wp->btns_h & WPAD_BUTTON_UP)
		J |= VBA_BUTTON_START;

	// Action
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_R;
	if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		if (fabs(wp->exp.nunchuk.gforce.x)> 0.6)
		{ // Wiiuse bug!!! Not correct values
			J |= VBA_BUTTON_B;
			systemGameRumble(20);
		}
		// CAKTODO hold down attack button to do spin attack

	}
	// Use item
	if (wp->btns_h & WPAD_BUTTON_B)
	{
		J |= VBA_BUTTON_A;
	}
	if (wp->btns_h & WPAD_BUTTON_LEFT)
	{
		J |= VBA_BUTTON_A;
	}
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
	{
		J |= VBA_BUTTON_A;
	}
	if (wp->btns_h & WPAD_BUTTON_DOWN)
	{
		J |= VBA_BUTTON_A;
	}
	// Menu (like Quest Status)
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Items
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;
	// Map
	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_L;
	// Sword
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_B;
	if (wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_BUTTON_B;
	// Shield has no controls
	// Camera (speed)
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
		J |= VBA_SPEED;
#endif

	return J;
}

u32 Zelda1Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u8 Health = 0;
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	// Talk to Midna, er... I mean ... nobody
	if (wp->btns_h & WPAD_BUTTON_UP)
		J |= 0;

	// Action
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		if (fabs(wp->exp.nunchuk.gforce.x)> 0.6)
		{ // Wiiuse bug!!! Not correct values
			J |= VBA_BUTTON_A;
			systemGameRumble(20);
		}
	}
	// Use item
	if (wp->btns_h & WPAD_BUTTON_B)
	{
		J |= VBA_BUTTON_B;
	}
	if (wp->btns_h & WPAD_BUTTON_LEFT)
	{
		J |= VBA_BUTTON_B;
	}
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
	{
		J |= VBA_BUTTON_A;
	}
	if (wp->btns_h & WPAD_BUTTON_DOWN)
	{
		J |= VBA_BUTTON_B;
	}
	// Menu (like Quest Status)
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_SELECT;
	// Items
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_START;
	// Map
	if (wp->btns_h & WPAD_BUTTON_1)
		J |= 0;
	// Sword
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_A;
	// Shield has no controls
	// Camera (speed)
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
		J |= VBA_SPEED;
#endif

	return J;
}

u32 Zelda2Input(unsigned short pad) {
	u32 J = StandardMovement(pad);
	u8 Health = 0;
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	// Talk to Midna, er... I mean ... nobody
	if (wp->btns_h & WPAD_BUTTON_UP)
		J |= 0;

	// Jump
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	// spin attack
	if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		if (fabs(wp->exp.nunchuk.gforce.x)> 0.6)
		{ // Wiiuse bug!!! Not correct values
			J |= VBA_BUTTON_B;
			systemGameRumble(20);
		}
	}
	// Use item
	if (wp->btns_h & WPAD_BUTTON_B)
	{
		J |= VBA_BUTTON_SELECT;
	}
	if (wp->btns_h & WPAD_BUTTON_LEFT)
	{
		J |= VBA_BUTTON_SELECT;
	}
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
	{
		J |= VBA_BUTTON_SELECT;
	}
	if (wp->btns_h & WPAD_BUTTON_DOWN)
	{
		J |= VBA_BUTTON_SELECT;
	}
	// Menu (like Quest Status)
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Items
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_START;
	// Map
	if (wp->btns_h & WPAD_BUTTON_1)
		J |= 0;
	// Sword
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_B;
	// No shield control, just duck
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_DOWN;
	// Camera (speed)
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
		J |= VBA_SPEED;
#endif
	return J;
}


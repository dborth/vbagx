/****************************************************************************
 * Visual Boy Advance GX
 *
 * Carl Kenner Febuary 2009
 * Daryl Borth 2026 (Decoupled Input Architecture)
 *
 * inputzelda.cpp
 *
 * Wii/Gamecube/Wii U controls for Legend of Zelda games
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
#include "libgui/Gui.h"
#include "vbasupport.h"

#include "vba/gba/GBA.h"
#include "vba/gba/bios.h"
#include "vba/gba/GBAinline.h"

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

u32 LinksAwakeningInput(unsigned short pad) // aka Zelda DX
{
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	u16 ItemsAddr = 0xDB00;
	static bool QuestScreen = false;
	static int StartCount = 0;
	static int SwordCount = 0;
	static bool BombArrows = false;
	static int DelayCount = 0;
	bool OnItemScreen = gbReadMemory(0xC16C) == 0x20; // 0x20 = items, 0x10 = normal

	u32 J = StandardMovement(pad);

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

	// 1. Unified hardware mapping.
	bool ActionButton       = data.buttons_h & GUI_BTN_A;
	bool SwordButton        = data.buttons_h & GUI_BTN_B;
	bool ShieldButton       = data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL);
	bool PullButton         = data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR);
	bool MidnaButton        = (data.buttons_h & GUI_BTN_UP) || data.substickY > 0.55f;
	bool UseLeftItemButton  = data.buttons_h & GUI_BTN_Y;
	bool UseRightItemButton = data.buttons_h & GUI_BTN_X;
	bool ItemsButton        = data.buttons_h & GUI_BTN_MINUS;
	bool QuestButton        = data.buttons_h & GUI_BTN_PLUS;
	bool MapButton          = data.buttons_h & GUI_BTN_RIGHT;
	bool SpeedButton        = data.buttons_h & GUI_BTN_DOWN;

	// D-Pad simulated items
	bool LeftItemButton     = data.substickX < -0.55f;
	bool DownItemButton     = data.substickY < -0.55f;
	bool RightItemButton    = data.substickX > 0.55f;
	bool BItemButton        = data.buttons_h & GUI_TRIGGER_R; // Fallback B-Item trigger

	// Motion Control Mappings
	if (data.hw_connected[GUI_HW_WIIMOTE]) {
		// Sword (Wiimote Shake)
		if (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5f && !OnItemScreen) {
			if (ZeldaDxDrawSword()) {
				if (SwordCount < 3) SwordCount = 3;
			}
			QuestScreen = false;
		}
	}
	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		// Spin Attack (Nunchuk Shake)
		if (fabs(data.hw_gforceX[GUI_HW_NUNCHUK]) > 0.6f && !OnItemScreen) {
			if (ZeldaDxDrawSword()) {
				if (SwordCount < 60) SwordCount = 60;
			}
			QuestScreen = false;
		}
	}

	if (SwordCount > 0) {
		if (SwordCount == 50)
			systemGameRumbleOnlyFor(50);
		if (!OnItemScreen)
			SwordButton = true;
		SwordCount--;
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
		if (ZeldaDxDrawSword()) J |= VBA_BUTTON_A;
	}

	// Pull button automatically switches to bracelet
	if (PullButton) {
		if (ZeldaDrawItem(3, ItemsAddr, 12)) J |= VBA_BUTTON_A;
	}

	// Shield and Z targetting
	if (ShieldButton && !OnItemScreen) {
		if (!SwordCount) {
			if (ZeldaDrawItem(4, ItemsAddr, 12)) J |= VBA_BUTTON_A;
		}
		QuestScreen = false;
	}

	// Z Button Selects bomb arrows on or off
	if (ShieldButton && OnItemScreen) {
		if (SelItem==2 || SelItem==5) { // toggle bomb arrows
			BombArrows = !BombArrows;
			if (BombArrows) systemGameRumbleOnlyFor(16);
			else systemGameRumbleOnlyFor(4);
			if (SelItem==2 && BombArrows) J |= VBA_BUTTON_A;
		} else if (BombArrows) { // switch off bomb arrows
			BombArrows = false;
			systemGameRumbleOnlyFor(4);
			J |= VBA_BUTTON_A;
		}
		QuestScreen = false;
	}

	static bool BIsLeft = true;
	if (UseLeftItemButton) {
		if (!BIsLeft) {
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
					J |= VBA_BUTTON_A; DelayCount++;
				}
			}
			else DelayCount = 10;
			if (DelayCount>1) J |= VBA_BUTTON_B;
		}
		QuestScreen = false;
	}

	if (UseRightItemButton) {
		if (BIsLeft) {
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
					J |= VBA_BUTTON_A; DelayCount++;
				}
			}
			else DelayCount = 10;
			if (DelayCount>1) J |= VBA_BUTTON_B;
		}
		QuestScreen = false;
	}

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
			if (DelayCount>1) J |= VBA_BUTTON_B;
		}
	}

	if (!BItemButton && !UseLeftItemButton && !UseRightItemButton)
		DelayCount = 0;

	// Talk to Midna (Save the game)
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
		if (StartCount>75) J |= VBA_BUTTON_START;
		StartCount--;
		if (StartCount==0) QuestScreen = true;
	}
	else if (StartCount<0) {
		QuestScreen = false;
		if (StartCount>=-5) J |= VBA_BUTTON_START;
		StartCount++;
	}

	if (QuestScreen && OnItemScreen)
		J |= VBA_BUTTON_SELECT;

	// Camera (fast forward)
	if (SpeedButton) {
		J |= VBA_SPEED;
		QuestScreen = false;
	}

	return J;
}

static u32 ZeldaOracleInput(bool Seasons, unsigned short pad) {
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	u16 ItemsAddr;
	if (Seasons) ItemsAddr = 0xC680;
	else ItemsAddr = 0xC688;
	static u32 OldJ = 0;

	u32 J = StandardMovement(pad);

	// Rumble when they lose health!
	u8 Health;
	if (Seasons) Health = gbReadMemory(0xC6A2); // health in quarters... note C6A3 is max health
	else Health = gbReadMemory(0xC6AA); // health in quarters... note C6AB is max health
	static u8 OldHealth = 0;
	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	static int DesiredSubscreen = -1;
	int Subscreen = 0;
	switch (gbReadMemory(0xCBCB)) {
		case 0: Subscreen = 0; break;
		case 1: Subscreen = 1+gbReadMemory(0xCBCF); break;
		case 2: Subscreen = 4; break;
		case 3: Subscreen = 5; break;
	}

	bool OnItemScreen = (Subscreen == 1);

	// 1. Unified hardware mapping.
	bool ActionButton       = data.buttons_h & GUI_BTN_A;
	bool SwordButton        = data.buttons_h & GUI_BTN_B;
	bool ShieldButton       = data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL);
	bool PullButton         = data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR);
	bool MidnaButton        = (data.buttons_h & GUI_BTN_UP) || data.substickY > 0.55f;
	bool UseLeftItemButton  = data.buttons_h & GUI_BTN_Y;
	bool UseRightItemButton = data.buttons_h & GUI_BTN_X;
	bool ItemsButton        = data.buttons_h & GUI_BTN_MINUS;
	bool QuestButton        = data.buttons_h & GUI_BTN_PLUS;
	bool MapButton          = data.buttons_h & GUI_BTN_RIGHT;
	bool SpeedButton        = data.buttons_h & GUI_BTN_DOWN;

	// D-Pad simulated items
	bool LeftItemButton     = data.substickX < -0.55f;
	bool DownItemButton     = data.substickY < -0.55f;
	bool RightItemButton    = data.substickX > 0.55f;
	bool BItemButton        = data.buttons_h & GUI_TRIGGER_R;

	// Motion Control Mappings
	static int SwordCount = 0;
	if (data.hw_connected[GUI_HW_WIIMOTE]) {
		if (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5f && !OnItemScreen) {
			if (SwordCount < 3) SwordCount = 3;
		}
	}
	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		if (fabs(data.hw_gforceX[GUI_HW_NUNCHUK]) > 0.6f && !OnItemScreen) {
			if (SwordCount < 60) SwordCount = 60;
		}
	}

	if (SwordCount > 0) {
		if (SwordCount == 50)
			systemGameRumbleOnlyFor(50);
		if (!OnItemScreen)
			SwordButton = true;
		SwordCount--;
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
		if (gbReadMemory(ItemsAddr+1)==5) SheathCount = 15;
	}

	u8 CursorPos = gbReadMemory(0xCBD0)+2;

	// Can't swap items if using two handed sword unless on item screen
	if (OnItemScreen || (gbReadMemory(ItemsAddr+0)!=0x0C && gbReadMemory(ItemsAddr+1)!=0x0C)) {
		// Left Item
		if (LeftItemButton) {
			if (OnItemScreen) ZeldaSwap(2, CursorPos, ItemsAddr);
			else ZeldaSwap(0, ZeldaDxLeftPos, ItemsAddr);
			systemGameRumbleOnlyFor(5);
		}
		// Right Item
		if (RightItemButton) {
			if (OnItemScreen) ZeldaSwap(4, CursorPos, ItemsAddr);
			else ZeldaSwap(0, ZeldaDxRightPos, ItemsAddr);
			systemGameRumbleOnlyFor(5);
		}
		// Down Item
		if (DownItemButton) {
			if (OnItemScreen) ZeldaSwap(3, CursorPos, ItemsAddr);
			else ZeldaSwap(0, ZeldaDxDownPos, ItemsAddr);
			systemGameRumbleOnlyFor(5);
		}
	}

	// B Item
	if (BItemButton) {
		if (OnItemScreen) systemGameRumbleOnlyFor(5);
		J |= VBA_BUTTON_B;
	}

	static bool BIsLeft = true;
	if (UseLeftItemButton) {
		if (!BIsLeft) {
			// Fix two-handed sword before swap
			if (gbReadMemory(ItemsAddr+0)==0x0C && gbReadMemory(ItemsAddr+1)==0x0C && !OnItemScreen) {
			} else {
				bool DrawingTwoHanded = (gbReadMemory(ItemsAddr+2)==0x0C);
				if (DrawingTwoHanded) DrawingTwoHanded = ZeldaDrawItem(0, ItemsAddr, 18); // put A item away (by drawing emptiness)
				ZeldaSwap(0, ZeldaDxLeftPos, ItemsAddr);
				if (DrawingTwoHanded) gbWriteMemory(ItemsAddr+1, 0x0C);
				BIsLeft = true;
			}
		}
		if (OnItemScreen) systemGameRumbleOnlyFor(5);
		J |= VBA_BUTTON_B;
	}

	if (UseRightItemButton) {
		if (BIsLeft) {
			// Fix two-handed sword before swap
			if (gbReadMemory(ItemsAddr+0)==0x0C && gbReadMemory(ItemsAddr+1)==0x0C && !OnItemScreen) {
			} else {
				bool DrawingTwoHanded = (gbReadMemory(ItemsAddr+2)==0x0C);
				if (DrawingTwoHanded) DrawingTwoHanded = ZeldaDrawItem(0, ItemsAddr, 18);
				ZeldaSwap(0, ZeldaDxLeftPos, ItemsAddr);
				if (DrawingTwoHanded) gbWriteMemory(ItemsAddr+1, 0x0C);
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
			if (gbReadMemory(ItemsAddr+1)!=0x0C && !ZeldaDrawItem(0, ItemsAddr, 18))
				ZeldaDrawItem(1, ItemsAddr, 18);
		}
		J |= VBA_BUTTON_A;
	}

	// Z-Targetting
	if (ShieldButton && !SwordButton) {
		// Fix two-handed sword before swap
		if (gbReadMemory(ItemsAddr+0)==0x0C && gbReadMemory(ItemsAddr+1)==0x0C)
			gbWriteMemory(ItemsAddr+0, 0);
		if (SheathCount>0) {
			ZeldaDrawItem(1, ItemsAddr, 18);
			SheathCount--;
		}
		else if (ZeldaDrawItem(1, ItemsAddr, 18)) J |= VBA_BUTTON_A;
	}

	// Pulling
	if (PullButton) {
		// Fix two-handed sword before swap
		if (gbReadMemory(ItemsAddr+0)==0x0C && gbReadMemory(ItemsAddr+1)==0x0C)
			gbWriteMemory(ItemsAddr+0, 0);
		if (ZeldaDrawItem(0x16, ItemsAddr, 18)) J |= VBA_BUTTON_A;
	}

	// Sword
	if (SwordButton) {
		if (ShieldButton && gbReadMemory(ItemsAddr+1)==1)
			ZeldaDrawItem(5, ItemsAddr, 18);
		else if (gbReadMemory(ItemsAddr)==0x0C || gbReadMemory(ItemsAddr+1)==0x0C || ZeldaDrawItem(5, ItemsAddr, 18))
			J |= VBA_BUTTON_A;
	}

	// Camera (fast forward)
	if (SpeedButton) J |= VBA_SPEED;

	if (DesiredSubscreen == Subscreen)
		DesiredSubscreen = -1;
	static int OldSubscreen = 0;
	if (Subscreen != OldSubscreen) {
		if (Subscreen==1) DelayCount = 80; // wait for items screen to fade in
		else DelayCount = 20; // wait to swap subscreens
	}
	OldSubscreen = Subscreen;

	if (DelayCount > 0) {
		DelayCount--;
	} else if (DesiredSubscreen == 0) { // game
		switch(Subscreen) {
			case 1: case 2: case 3: case 5: default: J |= VBA_BUTTON_START; break;
			case 4: J |= VBA_BUTTON_SELECT;
		}
	} else if (DesiredSubscreen == 1) { // items
		switch(Subscreen) {
			case 0: case 5: default: J |= VBA_BUTTON_START; break;
			case 2: case 3: case 4: J |= VBA_BUTTON_SELECT;
		}
	} else if (DesiredSubscreen == 2) { // secondary items
		switch(Subscreen) {
			case 0: case 5: default: J |= VBA_BUTTON_START; break;
			case 1: case 3: case 4: J |= VBA_BUTTON_SELECT;
		}
	} else if (DesiredSubscreen == 3) { // quest status
		switch(Subscreen) {
			case 0: case 5: default: J |= VBA_BUTTON_START; break;
			case 1: case 2: case 4: J |= VBA_BUTTON_SELECT;
		}
	} else if (DesiredSubscreen == 4) { // map
		switch(Subscreen) {
			case 0: J |= VBA_BUTTON_SELECT; break;
			case 1: case 2: case 3: case 5: default: J |= VBA_BUTTON_START;
		}
	} else if (DesiredSubscreen == 5) { // save
		switch(Subscreen) {
			case 0: default: J |= VBA_BUTTON_START; break;
			case 1: case 2: case 4: J |= VBA_BUTTON_SELECT; break;
			case 3:
				J |= VBA_RIGHT;
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
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	u32 J = StandardMovement(pad);

	// Rumble when they lose health!
	u8 Health = CPUReadByte(0x2002aea);
	static u8 OldHealth = 0;
	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	static u8 SubscreenWanted = 0xFF;
	static bool waiting = false;
	u8 Subscreen = CPUReadByte(0x200008C);
	if (Subscreen == 0x64) Subscreen = 0; // Boss battle (balloon)

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

	if (AButtonItem <= 6 && AButtonItem >= 1) SwordButtonNumber = VBA_BUTTON_A;
	else if (BButtonItem <= 6 && BButtonItem >= 1) SwordButtonNumber = VBA_BUTTON_B;
	else SwordButtonNumber = VBA_BUTTON_START;

	// Shield
	if (BButtonItem >= 0xD && BButtonItem <= 0xE) ZTargetButton = VBA_BUTTON_B;
	else if (AButtonItem >= 0xD && AButtonItem <= 0xE) ZTargetButton = VBA_BUTTON_A;
	// Gust Jar also does Z targetting
	else if (BButtonItem == 0x11) ZTargetButton = VBA_BUTTON_B;
	else if (AButtonItem == 0x11) ZTargetButton = VBA_BUTTON_A;
	else ZTargetButton = 0;

	// 1. Unified hardware mapping.
	bool ActionButton       = data.buttons_h & GUI_BTN_A;
	bool SwordButton        = data.buttons_h & GUI_BTN_B;
	bool ShieldButton       = data.buttons_h & (GUI_TRIGGER_L | GUI_TRIGGER_ZL);
	bool PullButton         = data.buttons_h & (GUI_TRIGGER_R | GUI_TRIGGER_ZR);
	bool MidnaButton        = (data.buttons_h & GUI_BTN_UP) || data.substickY > 0.55f;
	bool UseLeftItemButton  = data.buttons_h & GUI_BTN_Y;
	bool UseRightItemButton = data.buttons_h & GUI_BTN_X;
	bool ItemsButton        = data.buttons_h & GUI_BTN_MINUS;
	bool QuestButton        = data.buttons_h & GUI_BTN_PLUS;
	bool MapButton          = data.buttons_h & GUI_BTN_RIGHT;
	bool SpeedButton        = data.buttons_h & GUI_BTN_DOWN;

	// D-Pad simulated items
	bool LeftItemButton     = data.substickX < -0.55f;
	bool DownItemButton     = data.substickY < -0.55f;
	bool RightItemButton    = data.substickX > 0.55f;
	bool BItemButton        = data.buttons_h & GUI_TRIGGER_R;

	bool OnItemScreen = (Subscreen==0x2c);
	u8 RButtonAction = CPUReadByte(0x200af32);

	// Wii Pointer selection on item screen
	int cx, cy, SelRow, SelCol, CursorRow = 0xFF, CursorCol = 0xFF;
	static int OldCursorRow = 0xFF;
	static int OldCursorCol = 0xFF;

	CursorVisible = data.validPointer && ((Subscreen != 0 && Subscreen != 0x64) || LoadMenu == 2);
	if (CursorVisible) {
		cx = (int)((data.cursor_x * 240.0f) / 640.0f);
		cy = (int)((data.cursor_y * 160.0f) / 480.0f);
	} else {
		cx = -1;
		cy = -1;
	}

	if (Subscreen == 0x2c) {
		SelRow = SelBox / 4;
		SelCol = SelBox % 4;
		if (SelBox == 16) { SelRow = 3; SelCol = 4; }
		else if (SelRow < 3 && SelCol >= 2) SelCol++;

		if (cy >= 35 && cy <= 55) CursorRow = 0;
		else if (cy >= 59 && cy <= 79) CursorRow = 1;
		else if (cy >= 83 && cy <= 103) CursorRow = 2;
		else if (cy >= 107 && cy <= 127) CursorRow = 3;
		else CursorRow = 0xFF;

		if (CursorRow < 3) {
			if (cx >= 52 && cx <= 79) CursorCol = 0;
			else if (cx >= 88 && cx <= 116) CursorCol = 1;
			else if (cx >= 124 && cx <= 152) CursorCol = 3;
			else if (cx >= 160 && cx <= 188) CursorCol = 4;
			else CursorCol = 0xFF;
		}
		else {
			if (cx >= 157 && cx <= 204) CursorCol = 4;
			else if (cx >= 52 && cx < 152) CursorCol = (cx - 52) / 25;
			else CursorCol = 0xFF;
		}

		if (CursorCol != 0xFF && CursorRow != 0xFF && (CursorCol != SelCol || CursorRow != SelRow)) {
			if (CursorCol > SelCol) J |= VBA_RIGHT;
			else if (CursorCol < SelCol) J |= VBA_LEFT;
			else if (CursorRow > SelRow) J |= VBA_DOWN;
			else if (CursorRow < SelRow) J |= VBA_UP;
		}
	}
	else if (Subscreen == 0x38)
	{
		switch (SelBox) {
			case 0: case 1: case 10: case 9: case 11: SelRow = 0; break;
			case 12: SelRow = 1; break;
			case 2: case 3: case 13: case 14: case 15: SelRow = 2; break;
			case 6: case 7: case 8: case 4: case 5: SelRow = 3; break;
			default: SelRow = 0xFF; break;
		}
		switch (SelBox) {
			case 0: case 2: case 6: SelCol = 0; break;
			case 7: SelCol = 1; break;
			case 1: case 3: case 8: SelCol = 2; break;
			case 10: case 13: case 4: SelCol = 3; break;
			case 9: case 12: case 14: SelCol = 4; break;
			case 11: case 15: case 5: SelCol = 5; break;
			default: SelCol = 0xFF; break;
		}

		if (cy >= 109 && cy <= 130) {
			CursorRow = 3;
			if (cx >= 36 && cx <= 113) CursorCol = (cx - 36) / 26;
			else if (cx >= 118 && cx <= 165) CursorCol = 3;
			else if (cx >= 168 && cx <= 215) CursorCol = 5;
			else CursorCol = 0xFF;
		} else {
			if (cx >= 35 && cx <= 70) {
				CursorCol = 0;
				if (cy >= 40 && cy <= 73) CursorRow = 0;
				else if (cy >= 79 && cy <= 104) CursorRow = 2;
				else CursorRow = 0xFF;
			} else if (cx >= 78 && cx <= 113) {
				CursorCol = 2;
				if (cy >= 40 && cy <= 73) CursorRow = 0;
				else if (cy >= 79 && cy <= 104) CursorRow = 2;
				else CursorRow = 0xFF;
			} else {
				if (cx >= 125 && cx <= 148) {
					CursorCol = 3;
					if (cy >= 44 && cy <= 65) CursorRow = 0;
					else if (cy >= 83 && cy <= 104) CursorRow = 2;
					else CursorRow = 0xFF;
				} else if (cx >= 151 && cx <= 172) {
					CursorCol = 4;
					if (cy >= 31 && cy <= 53) CursorRow = 0;
					else if (cy >= 57 && cy <= 78) CursorRow = 1;
					else if (cy >= 83 && cy <= 104) CursorRow = 2;
					else CursorRow = 0xFF;
				} else if (cx >= 175 && cx <= 198) {
					CursorCol = 5;
					if (cy >= 44 && cy <= 65) CursorRow = 0;
					else if (cy >= 83 && cy <= 104) CursorRow = 2;
					else CursorRow = 0xFF;
				} else {
					CursorCol = 0xFF;
					CursorRow = 0xFF;
				}
			}
		}

		if (CursorCol != 0xFF && CursorRow != 0xFF && (CursorCol != SelCol || CursorRow != SelRow)) {
			if (CursorCol > SelCol) J |= VBA_RIGHT;
			else if (CursorCol < SelCol) J |= VBA_LEFT;
			else if (CursorRow > SelRow) J |= VBA_DOWN;
			else if (CursorRow < SelRow) J |= VBA_UP;
		}
	}

	OldCursorRow = CursorRow;
	OldCursorCol = CursorCol;

	// Motion Control Mappings
	static int SwordCount = 0;
	if (data.hw_connected[GUI_HW_WIIMOTE]) {
		if (fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5f && !OnItemScreen) {
			if (SwordCount < 3) SwordCount = 3;
		}
	}
	if (data.hw_connected[GUI_HW_NUNCHUK]) {
		// Throw gesture
		if (RButtonAction == 0x03) {
			if (fabs(data.hw_gforceY[GUI_HW_NUNCHUK]) > 0.6f) {
				J |= VBA_BUTTON_R;
				systemGameRumble(5);
			}
		// Spin attack
		} else if (fabs(data.hw_gforceX[GUI_HW_NUNCHUK]) > 0.6f && !OnItemScreen) {
			if (SwordCount < 60) SwordCount = 60;
		}
	}

	if (SwordCount > 0) {
		if (SwordCount == 50)
			systemGameRumbleOnlyFor(50);
		if (!OnItemScreen)
			SwordButton = true;
		SwordCount--;
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
		if (MidnaButton) J |= VBA_UP;
		if (DownItemButton) J |= VBA_DOWN;
		if (LeftItemButton) J |= VBA_LEFT;
		if (RightItemButton) J |= VBA_RIGHT;
		if (ActionButton) J |= VBA_BUTTON_A;
		if (BButtonItem) J |= VBA_BUTTON_B;
		if (QuestButton) J |= VBA_BUTTON_R;
		if (ItemsButton) J |= VBA_BUTTON_L;
		if (MapButton) J |= VBA_BUTTON_SELECT;
		if (SpeedButton) J |= VBA_BUTTON_SELECT;
		if (ShieldButton) J |= VBA_BUTTON_START;
		return J;
	}

	if (Subscreen == 0x70 && SubscreenWanted == 0x0c) {
		SubscreenWanted = 0xFF;
		waiting = false;
	}

	if (Subscreen != SubscreenWanted && SubscreenWanted != 0xFF) {
		frame++;
		if (frame % 2 == 0) {
			switch (Subscreen) {
				case 0:
					frame = 0;
					if (!waiting) {
						waiting = true;
						J |= VBA_BUTTON_START;
					}
					break;
				case 0x0c: // map
				case 0x70:
					if (SubscreenWanted == 0x38) J |= VBA_BUTTON_L;
					else if (SubscreenWanted == 0x2c) J |= VBA_BUTTON_R;
					else J |= VBA_BUTTON_START;
					waiting = false;
					break;
				case 0x2c: // items
					if (SubscreenWanted == 0x0c || SubscreenWanted == 0x70) J |= VBA_BUTTON_L;
					else if (SubscreenWanted == 0x38) J |= VBA_BUTTON_R;
					else J |= VBA_BUTTON_START;
					waiting = false;
					break;
				case 0x38: // quest
					if (SubscreenWanted == 0x2c) J |= VBA_BUTTON_L;
					else if (SubscreenWanted == 0x0c || SubscreenWanted == 0x70) J |= VBA_BUTTON_R;
					else J |= VBA_BUTTON_START;
					waiting = false;
					break;
				case 0x89: // kinstone
				case 0xF0: // sword techniques
					J |= VBA_BUTTON_B;
					waiting = false;
					break;
			}
		}
	} else {
		SubscreenWanted = 0xFF;
		waiting = false;
	}

	// Talk to Midna, er... I mean Ezlo
	if (MidnaButton) J |= VBA_BUTTON_SELECT;

	// Save button
	if ((Subscreen==0x2c && SelBox==16) || (Subscreen==0x38 && SelBox==5)) {
		if (ActionButton) {
			J |= VBA_BUTTON_A;
			systemGameRumble(12);
		}
	} else if (Subscreen==0x2c) {
	} else if (Subscreen==0x38 && SelBox==4) { // Sleep button returns to menu instead of sleep
		if (ActionButton) {
			MenuRequested = true;
			return 0;
		}
	} else if (Subscreen==0x38) {
		if (ActionButton) J |= VBA_BUTTON_A;
	} else {
		if (ActionButton) J |= VBA_BUTTON_R;
	}

	// Right Item
	if (RightItemButton || UseRightItemButton) {
		J |= VBA_BUTTON_A;
		if (Subscreen==0x2c) systemGameRumble(10);
	}
	// Down Item
	if (Subscreen==0x38) {
	} else {
		if (BItemButton) {
			J |= VBA_BUTTON_B;
			if (Subscreen==0x2c) systemGameRumble(10);
		}
	}
	if (DownItemButton || UseLeftItemButton) {
		J |= VBA_BUTTON_B;
		if (Subscreen==0x2c) systemGameRumble(10);
	}

	// Kinstone (doesn't work in items screen)
	if (LeftItemButton && Subscreen != 0x2c) J |= VBA_BUTTON_L;
	// Sword
	if (Subscreen == 0 && SwordButton) J |= SwordButtonNumber;
	// Z-Targetting
	if (Subscreen==0 && ShieldButton) J |= ZTargetButton;
	// Camera
	if (SpeedButton) J |= VBA_BUTTON_A;
	// Pull
	if (PullButton) J |= VBA_BUTTON_R;

	return J;
}

u32 ALinkToThePastInput(unsigned short pad)
{
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	u32 J = StandardMovement(pad);
	u8 Health = 0;
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	// Talk to Midna / Menu
	if ((data.buttons_h & GUI_BTN_UP) || (data.buttons_h & GUI_BTN_PLUS)) J |= VBA_BUTTON_START;
	// Action
	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_R;
	// Use item
	if ((data.buttons_h & GUI_BTN_B) || (data.buttons_h & GUI_BTN_LEFT) || (data.buttons_h & GUI_BTN_RIGHT) || (data.buttons_h & GUI_BTN_DOWN))
		J |= VBA_BUTTON_A;
	// Items
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	// Map
	if ((data.buttons_h & GUI_TRIGGER_L) || (data.buttons_h & GUI_TRIGGER_ZL)) J |= VBA_BUTTON_L;
	// Camera (speed)
	if ((data.buttons_h & GUI_TRIGGER_R) || (data.buttons_h & GUI_TRIGGER_ZR)) J |= VBA_SPEED;

	// Sword Generic
	if ((data.buttons_h & GUI_BTN_B) || (data.buttons_h & GUI_BTN_X) || (data.buttons_h & GUI_BTN_Y)) J |= VBA_BUTTON_B;

	// Sword / Spin Attack Motion Controls
	if (data.hw_connected[GUI_HW_NUNCHUK] && fabs(data.hw_gforceX[GUI_HW_NUNCHUK]) > 0.6f) {
		J |= VBA_BUTTON_B;
		systemGameRumble(20);
	}
	if (data.hw_connected[GUI_HW_WIIMOTE] && fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5f) {
		J |= VBA_BUTTON_B;
	}

	return J;
}

u32 Zelda1Input(unsigned short pad)
{
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	u32 J = StandardMovement(pad);
	u8 Health = 0;
	static u8 OldHealth = 0;

	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	// Action
	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	// Use item
	if ((data.buttons_h & GUI_BTN_B) || (data.buttons_h & GUI_BTN_LEFT) || (data.buttons_h & GUI_BTN_DOWN)) J |= VBA_BUTTON_B;
	if (data.buttons_h & GUI_BTN_RIGHT) J |= VBA_BUTTON_A;

	// Menu (like Quest Status)
	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_SELECT;
	// Items
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_START;
	// Sword Generic
	if ((data.buttons_h & GUI_BTN_B) || (data.buttons_h & GUI_BTN_X) || (data.buttons_h & GUI_BTN_Y)) J |= VBA_BUTTON_A;
	// Camera (speed)
	if ((data.buttons_h & GUI_TRIGGER_R) || (data.buttons_h & GUI_TRIGGER_ZR)) J |= VBA_SPEED;

	// Sword Motion Controls
	if (data.hw_connected[GUI_HW_NUNCHUK] && fabs(data.hw_gforceX[GUI_HW_NUNCHUK]) > 0.6f) {
		J |= VBA_BUTTON_A;
		systemGameRumble(20);
	}
	if (data.hw_connected[GUI_HW_WIIMOTE] && fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5f) {
		J |= VBA_BUTTON_A;
	}

	return J;
}

u32 Zelda2Input(unsigned short pad)
{
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	u32 J = StandardMovement(pad);
	u8 Health = 0;
	static u8 OldHealth = 0;

	if (Health < OldHealth) systemGameRumble(20);
	OldHealth = Health;

	// Jump
	if (data.buttons_h & GUI_BTN_A) J |= VBA_BUTTON_A;
	// Use item
	if ((data.buttons_h & GUI_BTN_B) || (data.buttons_h & GUI_BTN_LEFT) || (data.buttons_h & GUI_BTN_RIGHT) || (data.buttons_h & GUI_BTN_DOWN))
		J |= VBA_BUTTON_SELECT;
	// Menu (like Quest Status)
	if (data.buttons_h & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	// Items
	if (data.buttons_h & GUI_BTN_MINUS) J |= VBA_BUTTON_START;

	// Sword Generic
	if ((data.buttons_h & GUI_BTN_B) || (data.buttons_h & GUI_BTN_X) || (data.buttons_h & GUI_BTN_Y)) J |= VBA_BUTTON_B;

	// Sword / Spin Attack Motion Controls
	if (data.hw_connected[GUI_HW_NUNCHUK] && fabs(data.hw_gforceX[GUI_HW_NUNCHUK]) > 0.6f) {
		J |= VBA_BUTTON_B;
		systemGameRumble(20);
	}
	if (data.hw_connected[GUI_HW_WIIMOTE] && fabs(data.hw_gforceX[GUI_HW_WIIMOTE]) > 1.5f) {
		J |= VBA_BUTTON_B;
	}

	// No shield control, just duck
	if ((data.buttons_h & GUI_TRIGGER_ZL) || (data.buttons_h & GUI_TRIGGER_L)) J |= VBA_DOWN;
	// Camera (speed)
	if ((data.buttons_h & GUI_TRIGGER_R) || (data.buttons_h & GUI_TRIGGER_ZR)) J |= VBA_SPEED;

	return J;
}

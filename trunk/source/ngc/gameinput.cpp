/****************************************************************************
 * Visual Boy Advance GX
 *
 * Carl Kenner Febuary 2009
 *
 * gameinput.cpp
 *
 * Wii controls for individual games
 ***************************************************************************/

#ifdef HW_RVL

#include <gccore.h>
#include <stdio.h>
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

u8 ZeldaDxLeftPos = 2, ZeldaDxRightPos = 3, ZeldaDxDownPos = 4;
u8 ZeldaDxShieldPos = 5, ZeldaDxSwordPos = 5;

void ZeldaDxSwapBItem(u8 pos)
{
	u8 OldBItem = gbReadMemory(0xDB00 + 0);
	gbWriteMemory(0xDB00 + 0, gbReadMemory(0xDB00 + pos));
	gbWriteMemory(0xDB00 + pos, OldBItem);
}

void ZeldaDxSwap(u8 pos1, u8 pos2)
{
	u8 OldItem = gbReadMemory(0xDB00 + pos1);
	gbWriteMemory(0xDB00 + pos1, gbReadMemory(0xDB00 + pos2));
	gbWriteMemory(0xDB00 + pos2, OldItem);
}

bool ZeldaDxDrawShield()
{
	if (gbReadMemory(0xDB00 + 1) == 4)
		return true;
	ZeldaDxShieldPos = 0xFF;
	for (int i = 0; i <= 11; i++)
	{
		if (gbReadMemory(0xDB00 + i) == 4)
		{
			ZeldaDxShieldPos = i;
		}
	}
	if (ZeldaDxShieldPos == 0xFF)
		return false;
	if (ZeldaDxShieldPos != 1)
	{

		gbWriteMemory(0xDB00 + ZeldaDxShieldPos, gbReadMemory(0xDB00 + 1)); // put A item away
		gbWriteMemory(0xDB00 + 1, 4); // set A item to shield
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
	for (int i = 0; i <= 11; i++)
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
}

u32 LinksAwakeningInput(unsigned short pad)
{
	static bool QuestScreen = false;
	static int StartCount = 0;
	static int SwordCount = 0;
	static bool BombArrows = false;
	static int DelayCount = 0;
	bool OnItemScreen = gbReadMemory(0xC16C) == 0x20; // 0x20 = items, 0x10 = normal

	u32 J = StandardMovement(pad);

	u8 Health = gbReadMemory(0xDB5A);
	u8 CursorPos = gbReadMemory(0xC1B6) + 2;
	u8 SelItem = 0;
	if (CursorPos < 12)
		SelItem = gbReadMemory(0xDB00 + CursorPos);
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth) 
		systemGameRumble(20);
	OldHealth = Health;

	WPADData * wp = WPAD_Data(pad);

	// Action button, and put away sword
	if (wp->btns_h & WPAD_BUTTON_A)
	{
		if (QuestScreen && OnItemScreen)
		{
			if (StartCount>=0) StartCount = -80;
		}
		else
		{
			if (!OnItemScreen)
				ZeldaDxSheathSword();
			else
				systemGameRumble(5);
				J |= VBA_BUTTON_A;
		}
	}

	// Sword
	if (fabs(wp->gforce.x)> 1.5 && !OnItemScreen)
	{
		if (ZeldaDxDrawSword())
		{
			if (SwordCount<3) SwordCount = 3;
		}
		QuestScreen = false;
	}
	// Spin attack
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && fabs(wp->exp.nunchuk.gforce.x)> 0.6 && !OnItemScreen)
	{
		if (ZeldaDxDrawSword())
		{
			if (SwordCount<60) SwordCount=60;
		}
		QuestScreen = false;
	}
	if (SwordCount>0)
	{
		if (SwordCount == 50)
			systemGameRumbleOnlyFor(50);
		if (!OnItemScreen)
			J |= VBA_BUTTON_A;
		SwordCount--;
	}

	// Talk to Midna, er... I mean save the game
	if (wp->btns_h & WPAD_BUTTON_UP)
	{
		J |= VBA_BUTTON_A | VBA_BUTTON_B | VBA_BUTTON_START | VBA_BUTTON_SELECT;
		systemGameRumbleOnlyFor(5);
		QuestScreen = false;
	}

	// Left Item
	if (wp->btns_h & WPAD_BUTTON_LEFT)
	{
		if (OnItemScreen) ZeldaDxSwap(ZeldaDxLeftPos, CursorPos);
		else ZeldaDxSwapBItem(ZeldaDxLeftPos);
		systemGameRumbleOnlyFor(5);
		QuestScreen = false;
	}
	// Right Item
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
	{
		if (OnItemScreen) ZeldaDxSwap(ZeldaDxRightPos, CursorPos);
		else ZeldaDxSwapBItem(ZeldaDxRightPos);
		systemGameRumbleOnlyFor(5);
		QuestScreen = false;
	}
	// Down Item
	if (wp->btns_h & WPAD_BUTTON_DOWN)
	{
		if (OnItemScreen) ZeldaDxSwap(ZeldaDxDownPos, CursorPos);
		else ZeldaDxSwapBItem(ZeldaDxDownPos);
		systemGameRumbleOnlyFor(5);
		QuestScreen = false;
	}
	// B Item
	if (wp->btns_h & WPAD_BUTTON_B)
	{
		if (QuestScreen && OnItemScreen)
		{
			if (StartCount>=0) StartCount = -80;
		}
		else
		{
			if (OnItemScreen)
			{
				/*if (BombArrows) {
				 if (SelItem==5) { // bow
				 } else {
				 BombArrows = false;
				 }
				 }*/
				systemGameRumble(5);
				DelayCount = 10;
			}
			else
			{
				u8 BButtonItem = gbReadMemory(0xDB00);
				if (BombArrows && (BButtonItem==5))
				{
					if (ZeldaDxDrawBombs())
					{
						J |= VBA_BUTTON_A;
						DelayCount++;
					}
				}
				else DelayCount = 10;
			}
			if (DelayCount>1)
				J |= VBA_BUTTON_B;
		}
	}
	else DelayCount = 0;

	// Items
	if (wp->btns_h & WPAD_BUTTON_MINUS)
	{
		if (QuestScreen) QuestScreen = false;
		else J |= VBA_BUTTON_START;
	}
	// Quest
	if (wp->btns_h & WPAD_BUTTON_PLUS)
	{
		StartCount = 80;
		if (OnItemScreen) StartCount = -StartCount;
	}
	if (StartCount>0)
	{
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

	// Map
	if (wp->btns_h & WPAD_BUTTON_1)
	{
		QuestScreen = false;
		J |= VBA_BUTTON_SELECT;
	}

	// Cheat
	if (wp->btns_h & WPAD_BUTTON_2)
	{
		ZeldaDxCheat();
		QuestScreen = false;
	}

	// Z Button uses shield
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) && !OnItemScreen)
	{
		if (!SwordCount)
		{
			if (ZeldaDxDrawShield())
				J |= VBA_BUTTON_A;
		}
		QuestScreen = false;
	}
	// Z Button Selects bomb arrows on or off
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) && OnItemScreen)
	{
		if (SelItem==2 || SelItem==5)
		{ // toggle bomb arrows
			BombArrows = !BombArrows;
			if (BombArrows) systemGameRumbleOnlyFor(16);
			else systemGameRumbleOnlyFor(4);
			if (SelItem==2 && BombArrows)
				J |= VBA_BUTTON_A;
		}
		else if (BombArrows)
		{ // switch off bomb arrows
			BombArrows = false;
			systemGameRumbleOnlyFor(4);
			J |= VBA_BUTTON_A;
		}
		QuestScreen = false;
	}
	// Camera (fast forward)
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
	{
		J |= VBA_SPEED;
		QuestScreen = false;
	}

	return J;
}

u32 OracleOfAgesInput(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u32 jp = PAD_ButtonsHeld(pad);
	//static u8 SubscreenWanted = 0xFF;
	//static bool waiting = false;
	u8 Health = 0;
	static u8 OldHealth = 0;
	//static u8 AButtonItem = 0;
	//static u8 BButtonItem = 0;
	//static int frame = 0;
	u32 SwordButton = 0;
	u32 ZTargetButton = 0;
	//if (AButtonItem<=6 && AButtonItem>=1) SwordButton = VBA_BUTTON_A;
	//else if (BButtonItem<=6 && BButtonItem>=1) SwordButton = VBA_BUTTON_B;
	//else SwordButton = VBA_BUTTON_START;
	SwordButton = VBA_BUTTON_B;
	// Shield
	//if (BButtonItem>=0xD && BButtonItem<=0xE) ZTargetButton = VBA_BUTTON_B;
	//else if (AButtonItem>=0xD && AButtonItem<=0xE) ZTargetButton = VBA_BUTTON_A;
	ZTargetButton = VBA_BUTTON_A;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	WPADData * wp = WPAD_Data(pad);

	// Talk to Midna, er... I mean nobody
	if (wp->btns_h & WPAD_BUTTON_UP)
		J |= 0;

	// Right Item
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
	{
		J |= VBA_BUTTON_A;
	}
	// Down Item
	if (wp->btns_h & WPAD_BUTTON_DOWN)
	{
		J |= VBA_BUTTON_B;
	}
	// Action
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	// Kinstone (doesn't work)
	if (wp->btns_h & WPAD_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	// Menu
	if (wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_BUTTON_START;
	// Map
	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_SELECT;
	// Quest
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Items
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_START;
	// Sword
	if (fabs(wp->gforce.x)> 1.5)
		J |= SwordButton;
	// Z-Targetting
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= ZTargetButton;
	// Camera (fast forward)
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
		J |= VBA_SPEED;

	// Menu
	if (jp & PAD_BUTTON_START)
		J |= VBA_BUTTON_START;
	// Inventory
	if (jp & PAD_BUTTON_UP)
		J |= VBA_BUTTON_START;
	// Map
	if (jp & PAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_SELECT;
	// Kinstone
	if (jp & PAD_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	// Action
	if (jp & PAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	if (jp & PAD_TRIGGER_R)
		J |= VBA_BUTTON_A;
	// Sword
	if (jp & PAD_BUTTON_B)
		J |= SwordButton;
	// Right Item
	if (jp & PAD_BUTTON_X)
		J |= VBA_BUTTON_A;
	// left Item
	if (jp & PAD_BUTTON_Y)
		J |= VBA_BUTTON_B;
	// Talk to Midna, er... I mean...
	if (jp & PAD_TRIGGER_Z)
		J |= VBA_BUTTON_SELECT;
	// Z-Targetting
	if (jp & PAD_TRIGGER_L)
		J |= ZTargetButton;

	return J;
}

u32 MinishCapInput(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u32 jp = PAD_ButtonsHeld(pad);
	WPADData * wp = WPAD_Data(pad);
	static u8 SubscreenWanted = 0xFF;
	static bool waiting = false;
	u8 Subscreen = CPUReadByte(0x200008C);
	if (Subscreen == 0x64)
		Subscreen = 0; // Boss battle (balloon)

	u8 RButtonAction = CPUReadByte(0x200af32);
	u8 GameStart = CPUReadByte(0x2000086);

	u8 SelBox = CPUReadByte(0x2000083);
	u8 Health = CPUReadByte(0x2002aea);
	u8 LoadMenu = CPUReadByte(0x200AF57);
	static u8 OldHealth = 0;
	static u8 AButtonItem = 0;
	static u8 BButtonItem = 0;
	if (Subscreen == 0x2c)
	{
		AButtonItem = CPUReadByte(0x200af3c);
		BButtonItem = CPUReadByte(0x200af5c);
	}
	else if (Subscreen == 0)
	{
		AButtonItem = CPUReadByte(0x200af5c);
		BButtonItem = CPUReadByte(0x200af7c);
	}
	static int frame = 0;
	u32 SwordButton = 0;
	u32 ZTargetButton = 0;
	if (AButtonItem <= 6 && AButtonItem >= 1)
		SwordButton = VBA_BUTTON_A;
	else if (BButtonItem <= 6 && BButtonItem >= 1)
		SwordButton = VBA_BUTTON_B;
	else
		SwordButton = VBA_BUTTON_START;
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

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	int cx, cy, SelRow, SelCol, CursorRow = 0xFF, CursorCol = 0xFF;
	static int OldCursorRow = 0xFF;
	static int OldCursorCol = 0xFF;
	CursorVisible = ((Subscreen != 0 && Subscreen != 0x64) || LoadMenu == 2);
	if (CursorVisible)
	{
		cx = (CursorX * 240) / 640;
		cy = (CursorY * 160) / 480;
	}
	else
	{
		cx = -1;
		cy = -1;
	}
	// Wii Pointer selection on item screen
	if (Subscreen == 0x2c)
	{
		SelRow = SelBox / 4;
		SelCol = SelBox % 4;
		if (SelBox == 16)
		{
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
		if (cy >= 109 && cy <= 130)
		{
			CursorRow = 3;
			if (cx >= 36 && cx <= 113)
				CursorCol = (cx - 36) / 26;
			else if (cx >= 118 && cx <= 165)
				CursorCol = 3;
			else if (cx >= 168 && cx <= 215)
				CursorCol = 5;
			else
				CursorCol = 0xFF;
		}
		else
		{
			if (cx >= 35 && cx <= 70)
			{
				CursorCol = 0;
				if (cy >= 40 && cy <= 73)
					CursorRow = 0;
				else if (cy >= 79 && cy <= 104)
					CursorRow = 2;
				else
					CursorRow = 0xFF;
			}
			else if (cx >= 78 && cx <= 113)
			{
				CursorCol = 2;
				if (cy >= 40 && cy <= 73)
					CursorRow = 0;
				else if (cy >= 79 && cy <= 104)
					CursorRow = 2;
				else
					CursorRow = 0xFF;
			}
			else
			{
				if (cx >= 125 && cx <= 148)
				{
					CursorCol = 3;
					if (cy >= 44 && cy <= 65)
						CursorRow = 0;
					else if (cy >= 83 && cy <= 104)
						CursorRow = 2;
					else
						CursorRow = 0xFF;
				}
				else if (cx >= 151 && cx <= 172)
				{
					CursorCol = 4;
					if (cy >= 31 && cy <= 53)
						CursorRow = 0;
					else if (cy >= 57 && cy <= 78)
						CursorRow = 1;
					else if (cy >= 83 && cy <= 104)
						CursorRow = 2;
					else
						CursorRow = 0xFF;
				}
				else if (cx >= 175 && cx <= 198)
				{
					CursorCol = 5;
					if (cy >= 44 && cy <= 65)
						CursorRow = 0;
					else if (cy >= 83 && cy <= 104)
						CursorRow = 2;
					else
						CursorRow = 0xFF;
				}
				else
				{
					CursorCol = 0xFF;
					CursorRow = 0xFF;
				}
			}
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
	if ((CursorRow == 0xFF || CursorCol == 0xFF) && (OldCursorRow == 0xFF
			|| OldCursorCol == 0xFF))
	{
		// no change, still not pointing at anything
	}
	else if (CursorVisible && (CursorRow != OldCursorRow || CursorCol
			!= OldCursorCol))
	{
		// Cursor changed buttons, so rumble
		systemGameRumble(5);
	}
	OldCursorRow = CursorRow;
	OldCursorCol = CursorCol;

	if (wp->btns_h & WPAD_BUTTON_MINUS)
	{ // items
		if (Subscreen == 0x2c) SubscreenWanted = 0;
		else SubscreenWanted = 0x2c;
		waiting = false;
	}
	else if (wp->btns_h & WPAD_BUTTON_1)
	{ // map
		if (Subscreen == 0x0c || Subscreen==0x70) SubscreenWanted = 0;
		else SubscreenWanted = 0x0c;
		waiting = false;
	}
	else if (wp->btns_h & WPAD_BUTTON_PLUS)
	{ // quest
		if (Subscreen == 0x38) SubscreenWanted = 0;
		else SubscreenWanted = 0x38;
		waiting = false;
	}
	else if (wp->btns_h & WPAD_BUTTON_A && Subscreen == 0x2c && SelBox!=16)
	{
		SubscreenWanted = 0;
		waiting = false;
	}
	else if (wp->btns_h & WPAD_BUTTON_B && Subscreen == 0x38)
	{
		SubscreenWanted = 0;
		waiting = false;
	}
	if (GameStart==2 || GameStart==3)
	{
		SubscreenWanted = 0;
		waiting = false;
		if (wp->btns_h & WPAD_BUTTON_PLUS || (wp->btns_h & WPAD_BUTTON_A && wp->btns_h & WPAD_BUTTON_B))
			J |= VBA_BUTTON_START;
		return J;
	}
	else if (LoadMenu==2)
	{
		if (wp->btns_h & WPAD_BUTTON_UP)
			J |= VBA_UP;
		if (wp->btns_h & WPAD_BUTTON_DOWN)
			J |= VBA_DOWN;
		if (wp->btns_h & WPAD_BUTTON_LEFT)
			J |= VBA_LEFT;
		if (wp->btns_h & WPAD_BUTTON_RIGHT)
			J |= VBA_RIGHT;
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		if (wp->btns_h & WPAD_BUTTON_B)
			J |= VBA_BUTTON_B;

		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_L;
		if (wp->btns_h & WPAD_BUTTON_1)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_BUTTON_START;
		if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
			J |= VBA_BUTTON_SELECT;
		if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
			J |= VBA_BUTTON_START;
		return J;
	}

	//sprintf(DebugStr, "%f A=%02x B=%02x R=%02x s%02x,%02x, wait=%d", fabs(data.exp.nunchuk.gforce.x), AButtonItem, BButtonItem, RButtonAction, Subscreen, SubscreenWanted, waiting);

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
	if (Subscreen == 0x70 && SubscreenWanted == 0x0c)
	{
		SubscreenWanted = 0xFF;
		waiting = false;
	}

	if (Subscreen != SubscreenWanted && SubscreenWanted != 0xFF)
	{
		frame++;
		if (frame % 2 == 0)
			switch (Subscreen)
			{
				case 0:
					frame = 0;
					if (!waiting)
					{
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
	}
	else
	{
		SubscreenWanted = 0xFF;
		waiting = false;
	}

	/*** Report wp->btns_h buttons (gamepads) ***/

	// Talk to Midna, er... I mean Ezlo
	if (wp->btns_h & WPAD_BUTTON_UP)
	J |= VBA_BUTTON_SELECT;

	// Save button
	if ((Subscreen==0x2c && SelBox==16) || (Subscreen==0x38 && SelBox==5))
	{
		if (wp->btns_h & WPAD_BUTTON_A)
		{
			J |= VBA_BUTTON_A;
			systemGameRumble(12);
		}
	}
	else if (Subscreen==0x2c)
	{
	}
	else if (Subscreen==0x38 && SelBox==4)
	{ // Sleep button returns to menu instead of sleep
		if (wp->btns_h & WPAD_BUTTON_A)
		{
			ConfigRequested = 1;
			return 0;
		}
	}
	else
	{
		// Action
		if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_R;
		if (RButtonAction==0x03 && (wp->exp.type == WPAD_EXP_NUNCHUK))
		{
			if (fabs(wp->exp.nunchuk.gforce.y)> 0.6)
			{
				J |= VBA_BUTTON_R;
				systemGameRumble(5);
			}
		}
		else if (wp->exp.type == WPAD_EXP_NUNCHUK)
		{
			if (fabs(wp->exp.nunchuk.gforce.x)> 0.6)
			{ // Wiiuse bug!!! Not correct values
				J |= SwordButton;
				systemGameRumble(20);
			}
			// CAKTODO hold down attack button to do spin attack

		}
	}
	// Right Item
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
	{
		J |= VBA_BUTTON_A;
		if (Subscreen==0x2c)
		{
			systemGameRumble(10);
		}
	}
	// Down Item
	if (Subscreen==0x38)
	{
	}
	else
	{
		if (wp->btns_h & WPAD_BUTTON_B)
		{
			J |= VBA_BUTTON_B;
			if (Subscreen==0x2c)
			{
				systemGameRumble(10);
			}
		}
	}
	if (wp->btns_h & WPAD_BUTTON_DOWN)
	{
		J |= VBA_BUTTON_B;
		if (Subscreen==0x2c)
		{
			systemGameRumble(10);
		}
	}
	// Kinstone (doesn't work in items screen)
	if (wp->btns_h & WPAD_BUTTON_LEFT && Subscreen != 0x2c)
		J |= VBA_BUTTON_L;
	// Menu
	if (wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_BUTTON_START;
	// Sword
	if (Subscreen == 0 && fabs(wp->gforce.x)> 1.5)
		J |= SwordButton;
	// Z-Targetting
	if (Subscreen==0 && (wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= ZTargetButton;
	// Camera (use as extra way of pressing A without using DPad)
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
		J |= VBA_BUTTON_A;

	// Menu
	if (jp & PAD_BUTTON_START)
		J |= VBA_BUTTON_START;
	// Inventory
	if (jp & PAD_BUTTON_UP)
		J |= VBA_BUTTON_START;
	// Map
	if (jp & PAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_START;
	// Kinstone
	if (jp & PAD_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	// Action
	if (jp & PAD_BUTTON_A)
		J |= VBA_BUTTON_R;
	if (jp & PAD_TRIGGER_R)
		J |= VBA_BUTTON_R;
	// Sword
	if (jp & PAD_BUTTON_B)
		J |= VBA_BUTTON_B;
	// Right Item
	if (jp & PAD_BUTTON_X)
		J |= VBA_BUTTON_A;
	// left Item
	if (jp & PAD_BUTTON_Y)
		J |= VBA_BUTTON_B;
	// Talk to Midna, er... I mean Ezlo
	if (jp & PAD_TRIGGER_Z)
		J |= VBA_BUTTON_SELECT;
	// Z-Targetting
	if (jp & PAD_TRIGGER_L)
		J |= VBA_BUTTON_A;

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

	return J;
}

u32 Zelda2Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u8 Health = 0;
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

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


	return J;
}

u32 MK1Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u8 Health = gbReadMemory(0xc695);
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(5);
	OldHealth = Health;

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

	return J;
}

u32 MK4Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u8 Health = 0;
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

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

	return J;
}

u32 MKAInput(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u8 Health = 0;
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

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

	return J;
}

u32 MKTEInput(unsigned short pad)
{
	static u32 prevJ = 0, prevPrevJ = 0;
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);
	u8 Health;
	u8 Side;
	if (RomIdCode & 0xFFFFFF == MKDA)
	{
		Health = CPUReadByte(0x3000760); // 731 or 760
		Side = CPUReadByte(0x3000747);
	}
	else
	{
		Health = CPUReadByte(0x3000761); // or 790
		Side = CPUReadByte(0x3000777);
	}
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	u32 Forwards, Back;
	if (Side == 0)
	{
		Forwards = VBA_RIGHT;
		Back = VBA_LEFT;
	}
	else
	{
		Forwards = VBA_LEFT;
		Back = VBA_RIGHT;
	}

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
	if (wp->btns_h & WPAD_BUTTON_A)
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
	// Speed
	if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
	{
		J |= VBA_SPEED;
	}

	if ((J & 48) == 48)
		J &= ~16;
	if ((J & 192) == 192)
		J &= ~128;
	prevPrevJ = prevJ;
	prevJ = J;

	return J;
}

u32 MarioKartInput(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u32 jp = PAD_ButtonsHeld(pad);
	WPADData * wp = WPAD_Data(pad);
	static u32 frame = 0;

	u8 Health = 0;
	static u8 OldHealth = 0;
	float fraction;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	// Start/Select
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	if (wp->exp.type == WPAD_EXP_NONE)
	{
		// Use item
		if (wp->btns_h & WPAD_BUTTON_RIGHT)
		{
			J |= VBA_BUTTON_L | VBA_UP;
			J &= ~VBA_DOWN;
		}
		else if (wp->btns_h & WPAD_BUTTON_LEFT)
		{
			J |= VBA_BUTTON_L | VBA_DOWN;
			J &= ~VBA_UP;
		}
		else if (wp->btns_h & WPAD_BUTTON_UP || wp->btns_h & WPAD_BUTTON_DOWN)
			J |= VBA_BUTTON_L;
		// Accelerate
		if (wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_BUTTON_A;
		// Brake
		if (wp->btns_h & WPAD_BUTTON_1)
			J |= VBA_BUTTON_B;
		// Jump/Power slide
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & WPAD_BUTTON_B)
			J |= VBA_BUTTON_R;
		if (wp->orient.pitch> 12)
		{
			fraction = (wp->orient.pitch - 12)/60.0f;
			if ((frame % 60)/60.0f < fraction)
				J |= VBA_LEFT;
		}
		else if (wp->orient.pitch <- 12)
		{
			fraction = -(wp->orient.pitch + 10)/60.0f;
			if ((frame % 60)/60.0f < fraction)
				J |= VBA_RIGHT;
		}

	}
	else if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		// Use item
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z)
			J |= VBA_BUTTON_L;
		// Accelerate
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Brake
		if (wp->btns_h & WPAD_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Jump
		if (fabs(wp->gforce.y)> 1.5 )
			J |= VBA_BUTTON_R;
		if (wp->btns_h & WPAD_BUTTON_UP || wp->btns_h & WPAD_BUTTON_LEFT || wp->btns_h & WPAD_BUTTON_RIGHT || wp->btns_h & WPAD_BUTTON_DOWN)
			J |= VBA_BUTTON_R;
	}
	else if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		// Use item
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L)
			J |= VBA_BUTTON_L;
		// Accelerate
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Brake
		if (wp->btns_h & WPAD_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Jump
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_UP || wp->btns_h & WPAD_CLASSIC_BUTTON_LEFT || wp->btns_h & WPAD_CLASSIC_BUTTON_RIGHT || wp->btns_h & WPAD_CLASSIC_BUTTON_DOWN)
			J |= VBA_BUTTON_R;
		// Start/Select
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	}


	if (jp & PAD_BUTTON_START)
		J |= VBA_BUTTON_START;
	if (jp & PAD_BUTTON_X || jp & PAD_BUTTON_Y)
		J |= VBA_BUTTON_SELECT;
	if (jp & PAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	if (jp & PAD_BUTTON_B)
		J |= VBA_BUTTON_B;
	if (jp & PAD_TRIGGER_L)
		J |= VBA_BUTTON_L;
	if (jp & PAD_TRIGGER_R)
		J |= VBA_BUTTON_R;
	if (jp & PAD_BUTTON_UP || jp & PAD_BUTTON_DOWN || jp & PAD_BUTTON_LEFT
			|| jp & PAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;

	frame++;

	return J;
}

u32 LegoStarWars1Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);

	u8 Health = 0;
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	WPADData * wp = WPAD_Data(pad);

	// Start/Select
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;

	if (wp->exp.type == WPAD_EXP_NONE)
	{
		// CAKTODO
		J |= StandardSideways(pad);
	}
	else if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		// build, use force
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z)
			J |= VBA_BUTTON_L;
		// change characters
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Shoot
		if (wp->btns_h & WPAD_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 )
			J |= VBA_BUTTON_B;
		// Force power, special ability
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
	}
	else if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		// CAKTODO same as game boy for now
		J |= StandardClassic(pad);
	}

	// CAKTODO same as game boy for now
	J |= StandardGamecube(pad) | StandardKeyboard(pad);

	return J;
}

u32 LegoStarWars2Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u8 Health = 0;
	static u8 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	WPADData * wp = WPAD_Data(pad);

	// Start/Select
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;

	if (wp->exp.type == WPAD_EXP_NONE)
	{
		J |= StandardSideways(pad);
	}
	else if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		// build, force transform, pull lever
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z)
			J |= VBA_BUTTON_R;
		// change characters
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C || wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_BUTTON_SELECT;
		// grapple
		if (fabs(wp->gforce.y)> 1.6 )
			J |= VBA_BUTTON_R;
		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Shoot
		if (wp->btns_h & WPAD_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 )
			J |= VBA_BUTTON_B;
		// Force power, vehicle special ability
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_L;
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;

	}
	else if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		// CAKTODO same as game boy for now
		J |= StandardClassic(pad);
	}

	// CAKTODO same as game boy for now
	J |= StandardGamecube(pad) | StandardKeyboard(pad);

	return J;
}

u32 MetroidZeroInput(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u32 jp = PAD_ButtonsHeld(pad);
	u8 BallState = CPUReadByte(0x30015df); // 0 = stand, 1 = crouch, 2 = ball
	u16 Health = CPUReadByte(0x3001536); // 0 = stand, 1 = crouch, 2 = ball
	static u16 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;
	static int AimCount = 0;
	static int MissileCount = 0;

	if (BallState == 2) // Can't exit morph ball without pressing C!
		J &= ~VBA_UP;
	if (BallState == 1) // Can't enter morph ball without pressing C!
		J &= ~VBA_DOWN;

	WPADData * wp = WPAD_Data(pad);

	if (wp->btns_h & WPAD_BUTTON_UP)
		J |= VBA_SPEED;
	if (wp->btns_h & WPAD_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;
	// Visor doesn't exist, use as start button
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_START;
	// Hyper (toggle super missiles)
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_SELECT;
	// Map
	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_START;
	// Hint
	if (wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_BUTTON_R;
	// Z-Target
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		;

	// Jump
	if (wp->btns_h & WPAD_BUTTON_B && BallState!=2)
		J |= VBA_BUTTON_A;
	else if (BallState==2 && fabs(wp->gforce.y)> 1.5)
		J |= VBA_BUTTON_A;
	// Fire
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_B;
	// Aim
	/*if (CursorValid && CursorY < 96 && BallState!=2)
	 J |= VBA_UP;
	 else if (CursorValid && CursorY < 192 && BallState!=2)
	 J |= VBA_BUTTON_L;
	 else if (CursorValid && CursorY < 288 && BallState!=2)
	 J |= 0;
	 else if (CursorValid && CursorY < 384 && BallState!=2)
	 J |= VBA_BUTTON_L & VBA_DOWN;
	 else if (CursorValid && BallState==0)
	 J |= VBA_DOWN;*/
	if (wp->orient.pitch < -45 && BallState !=2)
	{
		J |= VBA_UP;
		AimCount = 0;
	}
	else if (wp->orient.pitch < -22 && BallState !=2)
	{
		if (AimCount>=0) AimCount = -1;
	}
	else if (wp->orient.pitch> 45 && BallState ==0)
	{
		if (AimCount<10) AimCount=10;
	}
	else if (wp->orient.pitch> 22 && BallState !=2)
	{
		if (AimCount<=0 || AimCount>=10) AimCount = 1;
	}
	else
	{
		AimCount=0;
	}

	// Morph Bdall
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_d & WPAD_NUNCHUK_BUTTON_C))
	{
		if (BallState == 2)
		{ // ball
			Morph = -1;
		}
		else if (BallState == 1)
		{
			Morph = 2;
		}
		else
		{
			Morph = 1;
		}
	}
	// Missile
	if (wp->btns_h & WPAD_BUTTON_DOWN)
	{
		MissileCount = 1;
	}

	switch (AimCount)
	{
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

	switch (MissileCount)
	{
		case 1:
		case 2:
			J |= VBA_BUTTON_R;
			MissileCount++;
			break;
		case 3:
		case 4:
			J |= VBA_BUTTON_R | VBA_BUTTON_B;
			MissileCount++;
			break;
		case 5:
			MissileCount = 0;
			break;
	}

	switch (Morph)
	{
		case 1:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_DOWN;
			Morph = 2;
			break;
		case 2:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			Morph = 3;
			break;
		case 3:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_DOWN;
			Morph = 0;
			break;
		case -1:
		case -2:
		case -3:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_UP;
			Morph--;
			break;
		case -4:
		case -5:
		case -6:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			Morph--;
			break;
		case -7:
		case -8:
		case -9:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_UP;
			Morph--;
			break;
		case -10:
			Morph = 0;
			break;
	}

	// Menu
	if (jp & PAD_BUTTON_START)
		J |= VBA_BUTTON_START;

	return J;
}

u32 MetroidFusionInput(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u32 jp = PAD_ButtonsHeld(pad);
	u8 BallState = CPUReadByte(0x3001329); // 0 = stand, 2 = crouch, 5 = ball
	u16 Health = CPUReadHalfWord(0x3001310); 
	static u16 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;
	static int AimCount = 0;
	static int MissileCount = 0;

	if (BallState == 5) // Can't exit morph ball without pressing C!
		J &= ~VBA_UP;
	if (BallState == 2) // Can't enter morph ball without pressing C!
		J &= ~VBA_DOWN;

	WPADData * wp = WPAD_Data(pad);

	if (wp->btns_h & WPAD_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;

	// Visor doesn't exist, just use as select button
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;
	// Hyper isn't available, just use start button as start
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map
	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_START;
	// Hint
	if (wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_BUTTON_R;
	// Z-Target
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_SPEED;

	// Jump
	if (wp->btns_h & WPAD_BUTTON_B && BallState!=5)
		J |= VBA_BUTTON_A;
	else if (BallState==5 && fabs(wp->gforce.y)> 1.5)
		J |= VBA_BUTTON_A;
	// Fire
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_B;
	// Aim
	if (wp->orient.pitch < -45 && BallState !=5)
	{
		J |= VBA_UP;
		AimCount = 0;
	}
	else if (wp->orient.pitch < -22 && BallState !=5)
	{
		if (AimCount>=0) AimCount = -1;
	}
	else if (wp->orient.pitch> 45 && BallState !=5)
	{
		if (AimCount<10) AimCount=10;
	}
	else if (wp->orient.pitch> 22 && BallState !=5)
	{
		if (AimCount<=0 || AimCount>=10) AimCount = 1;
	}
	else
	{
		AimCount=0;
	}

	// Morph Bdall
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_d & WPAD_NUNCHUK_BUTTON_C))
	{
		if (BallState == 5)
		{ // ball
			Morph = -1;
		}
		else if (BallState == 2)
		{
			Morph = 2;
		}
		else
		{
			Morph = 1;
		}
	}
	// Missile
	if (wp->btns_h & WPAD_BUTTON_DOWN)
	{
		MissileCount = 1;
	}

	switch (AimCount)
	{
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

	switch (MissileCount)
	{
		case 1:
		case 2:
			J |= VBA_BUTTON_R;
			MissileCount++;
			break;
		case 3:
		case 4:
			J |= VBA_BUTTON_R | VBA_BUTTON_B;
			MissileCount++;
			break;
		case 5:
			MissileCount = 0;
			break;
	}

	switch (Morph)
	{
		case 1:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_DOWN;
			Morph = 2;
			break;
		case 2:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			Morph = 3;
			break;
		case 3:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_DOWN;
			Morph = 0;
			break;
		case -1:
		case -2:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_UP;
			Morph--;
			break;
		case -3:
		case -4:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			Morph--;
			break;
		case -5:
		case -6:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_UP;
			Morph--;
			break;
		case -7:
			Morph = 0;
			break;
	}

	// Menu
	if (jp & PAD_BUTTON_START)
		J |= VBA_BUTTON_START;

	return J;
}

u32 Metroid1Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u32 jp = PAD_ButtonsHeld(pad);
	u8 BallState = CPUReadByte(0x3007500); // 3 = ball, other = stand
	u8 MissileState = CPUReadByte(0x300730E); // 1 = missile, 0 = beam
	u16 Health = CPUReadHalfWord(0x3007306); // Binary Coded Decimal
	static u16 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;

	if (BallState == 3) // Can't exit morph ball without pressing C!
		J &= ~VBA_UP;
	if (BallState != 3) // Can't enter morph ball without pressing C!
		J &= ~VBA_DOWN;

	WPADData * wp = WPAD_Data(pad);

	// No visors
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;
	// Start button just pauses
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map (doesn't exist)
	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_START;
	// Hint (doesn't exist)
	if (wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_SPEED;
	// Z-Target
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		;

	// Jump
	if (wp->btns_h & WPAD_BUTTON_B && BallState!=5)
		J |= VBA_BUTTON_A;
	else if (BallState==5 && fabs(wp->gforce.y)> 1.5)
		J |= VBA_BUTTON_A;
	// Fire
	if (wp->btns_h & WPAD_BUTTON_A) {
		if (MissileState)
			J |= VBA_BUTTON_SELECT;
		else
			J |= VBA_BUTTON_B;
	}
	
	// Aim
	if (wp->orient.pitch < -45 && BallState !=3)
		J |= VBA_UP;

	// Morph Ball
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_d & WPAD_NUNCHUK_BUTTON_C))
	{
		if (BallState == 3)
		{ // ball
			Morph = -1;
		}
		else
		{
			Morph = 2;
		}
	}
	// Missile
	if (wp->btns_h & WPAD_BUTTON_DOWN)
	{
		if (!(MissileState))
			J |= VBA_BUTTON_SELECT;
		else
			J |= VBA_BUTTON_B;
	}
	if (wp->btns_u & WPAD_BUTTON_DOWN)
	{
		if (MissileState)
			J |= VBA_BUTTON_SELECT;
	}

	switch (Morph)
	{
		case 1:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_DOWN;
			Morph = 2;
			break;
		case 2:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			Morph = 3;
			break;
		case 3:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_DOWN;
			Morph = 0;
			break;
		case -1:
		case -2:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_UP;
			Morph--;
			break;
		case -3:
		case -4:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			Morph--;
			break;
		case -5:
		case -6:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_UP;
			Morph--;
			break;
		case -7:
			Morph = 0;
			break;
	}

	// Menu
	if (jp & PAD_BUTTON_START)
		J |= VBA_BUTTON_START;

	return J;
}

u32 Metroid2Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	u32 jp = PAD_ButtonsHeld(pad);
	u8 BallState = gbReadMemory(0xD020); // 4 = crouch, 5 = ball, other = stand
	u8 MissileState = gbReadMemory(0xD04D); // 8 = missile hatch open, 0 = missile closed
	u8 Health = gbReadMemory(0xD051); // Binary Coded Decimal
	static u8 OldHealth = 0;

	// Rumble when they lose (or gain) health! (since I'm not checking energy tanks)
	if (Health != OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;
	static int AimCount = 0;

	if (BallState == 5) // Can't exit morph ball without pressing C!
		J &= ~VBA_UP;
	if (BallState == 4) // Can't enter morph ball without pressing C!
		J &= ~VBA_DOWN;

	WPADData * wp = WPAD_Data(pad);

	// No visors
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;
	// Start button just pauses
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map (doesn't exist)
	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_START;
	// Hint (doesn't exist)
	if (wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_SPEED;
	// Z-Target
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		;

	// Jump
	if (wp->btns_h & WPAD_BUTTON_B && BallState!=5)
		J |= VBA_BUTTON_A;
	else if (BallState==5 && fabs(wp->gforce.y)> 1.5)
		J |= VBA_BUTTON_A;
	// Fire
	if (wp->btns_h & WPAD_BUTTON_A) {
		if (MissileState & 8)
			J |= VBA_BUTTON_SELECT;
		else
			J |= VBA_BUTTON_B;
	}
	
	// Aim
	/*if (CursorValid && CursorY < 96 && BallState!=2)
	 J |= VBA_UP;
	 else if (CursorValid && CursorY < 192 && BallState!=2)
	 J |= VBA_BUTTON_L;
	 else if (CursorValid && CursorY < 288 && BallState!=2)
	 J |= 0;
	 else if (CursorValid && CursorY < 384 && BallState!=2)
	 J |= VBA_BUTTON_L & VBA_DOWN;
	 else if (CursorValid && BallState==0)
	 J |= VBA_DOWN;*/
	if (wp->orient.pitch < -45 && BallState !=5)
	{
		J |= VBA_UP;
		AimCount = 0;
	}
	else if (wp->orient.pitch> 45 && BallState !=5 && BallState !=4)
	{
		//if (AimCount<10) AimCount=10;
	}
	else
	{
		AimCount=0;
	}

	// Morph Ball
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_d & WPAD_NUNCHUK_BUTTON_C))
	{
		if (BallState == 5)
		{ // ball
			Morph = -1;
		}
		else if (BallState == 4)
		{
			Morph = 2;
		}
		else
		{
			Morph = 1;
		}
	}
	// Missile
	if (wp->btns_h & WPAD_BUTTON_DOWN)
	{
		if (!(MissileState & 8))
			J |= VBA_BUTTON_SELECT;
		else
			J |= VBA_BUTTON_B;
	}
	if (wp->btns_u & WPAD_BUTTON_DOWN)
	{
		if (MissileState & 8)
			J |= VBA_BUTTON_SELECT;
	}

	switch (Morph)
	{
		case 1:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_DOWN;
			Morph = 2;
			break;
		case 2:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			Morph = 3;
			break;
		case 3:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_DOWN;
			Morph = 0;
			break;
		case -1:
		case -2:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_UP;
			Morph--;
			break;
		case -3:
		case -4:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			Morph--;
			break;
		case -5:
		case -6:
			J &= ~(VBA_UP | VBA_DOWN | VBA_LEFT | VBA_RIGHT | VBA_BUTTON_L);
			J |= VBA_UP;
			Morph--;
			break;
		case -7:
			Morph = 0;
			break;
	}

	// Menu
	if (jp & PAD_BUTTON_START)
		J |= VBA_BUTTON_START;

	return J;
}

u32 TMNTInput(unsigned short pad)
{
	u32 J = StandardMovement(pad) | StandardDPad(pad);
	//u32 jp = PAD_ButtonsHeld(pad);

	WPADData * wp = WPAD_Data(pad);

	// Jump
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	// Attack
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_B;
	// Kick Attack
	if (fabs(wp->exp.nunchuk.gforce.x)> 0.5)
		J |= VBA_BUTTON_B;

	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_BUTTON_B; // Double tap D-Pad to roll CAKTODO


	// Swap Turtles
	if (wp->btns_h & WPAD_BUTTON_B)
		J |= VBA_BUTTON_R;
	// Pause
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Select
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;
	// Roll
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
		J |= 0; // Double tap D-Pad to roll CAKTODO

	// CAKTODO
	return J;
}

u32 HarryPotter1GBCInput(unsigned short pad)
{
	u32 J = StandardMovement(pad) | StandardDPad(pad);
	//u8 ScreenMode = gbReadMemory(0xFFCF);
	//u8 CursorItem = gbReadMemory(0xFFD5);

	WPADData * wp = WPAD_Data(pad);

	// Pause Menu
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map (well, it tells you where you are)
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// talk or interact
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	// cancel
	if (wp->btns_h & WPAD_BUTTON_B)
		J |= VBA_BUTTON_B;
	// spells
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_A;

	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_R;

	// Run (uses emulator speed button)
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_SPEED;
	// Camera, just tells you what room you are in. CAKTODO make press and release trigger it
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
		J |= VBA_BUTTON_SELECT;

	// CAKTODO spell gestures

	return J;
}

u32 HarryPotter2GBCInput(unsigned short pad)
{
	u32 J = StandardMovement(pad) | StandardDPad(pad);
	WPADData * wp = WPAD_Data(pad);

	// Pause Menu
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map (well, it tells you where you are)
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// talk or interact
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	// cancel
	if (wp->btns_h & WPAD_BUTTON_B)
		J |= VBA_BUTTON_B;
	// spells
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_A;

	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_R;

	// Run (uses emulator speed button)
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_SPEED;
	// Camera, just tells you what room you are in. CAKTODO make press and release trigger it
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
		J |= VBA_BUTTON_SELECT;

	// CAKTODO spell gestures

	return J;
}

u32 HarryPotter1Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);

	// DPad works in the map
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;
	if (wp->btns_h & WPAD_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_UP)
		J |= VBA_UP;
	if (wp->btns_h & WPAD_BUTTON_DOWN)
		J |= VBA_DOWN;

	// Pause
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// talk or interact
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	// spells
	if (wp->btns_h & WPAD_BUTTON_B)
		J |= VBA_BUTTON_B;
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_B;

	if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_BUTTON_R;

	// Run (uses emulator speed button)
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_SPEED;
	// Flute
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
		J |= VBA_BUTTON_L;

	// CAKTODO spell gestures


	return J;
}

u32 HarryPotter2Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);

	// DPad works in the map
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;
	if (wp->btns_h & WPAD_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_UP)
		J |= VBA_UP;
	if (wp->btns_h & WPAD_BUTTON_DOWN)
		J |= VBA_DOWN;

	// Pause
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// talk or interact or sneak
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_B;
	// spells
	if (wp->btns_h & WPAD_BUTTON_B)
		J |= VBA_BUTTON_A;
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_A;

	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_BUTTON_L;

	// Sneak instead of run
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_BUTTON_B;
	// Jump with C button
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
		J |= VBA_BUTTON_R;

	// CAKTODO spell gestures


	return J;
}

u32 HarryPotter3Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);

	// DPad works in the map
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;
	if (wp->btns_h & WPAD_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_UP)
		J |= VBA_UP;
	if (wp->btns_h & WPAD_BUTTON_DOWN)
		J |= VBA_DOWN;

	// Pause
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map doesn't exist. Options instead
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// talk or interact
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	// spells
	if (wp->btns_h & WPAD_BUTTON_B)
		J |= VBA_BUTTON_B;
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_B;

	// Change spells
	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_BUTTON_R;

	// Run (uses emulator speed button)
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_SPEED;

	// CAKTODO spell gestures
	// swing sideways for Flipendo
	// point at ceiling for Lumos


	return J;
}

u32 HarryPotter4Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);

	// DPad works in the map
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;
	if (wp->btns_h & WPAD_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_UP)
		J |= VBA_UP;
	if (wp->btns_h & WPAD_BUTTON_DOWN)
		J |= VBA_DOWN;

	// Pause
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map doesn't exist. Select Needed for starting game.
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// talk or interact or jinx
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_A;
	// Charms
	if (wp->btns_h & WPAD_BUTTON_B)
		J |= VBA_BUTTON_B;

	// L and R
	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_BUTTON_R;

	// Run (uses emulator speed button)
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_SPEED;

	// CAKTODO spell gestures
	// swing sideways for Flipendo
	// point at ceiling for Lumos


	return J;
}

u32 HarryPotter5Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);

	// Wand cursor, normally controlled by DPad, now controlled by Wiimote!
	int cx = 0;
	int cy = 0;
	static int oldcx = 0;
	static int oldcy = 0;
	u8 WandOut = CPUReadByte(0x200e0dd);
	if (WandOut && CursorValid)
	{
		cx = (CursorX * 268)/640;
		cy = (CursorY * 187)/480;
		if (cx<0x14) cx=0x14;
		else if (cx>0xf8) cx=0xf8;
		if (cy<0x13) cy=0x13;
		else if (cy>0xa8) cy=0xa8;
		CPUWriteByte(0x200e0fe, cx);
		CPUWriteByte(0x200e102, cy);
	}
	oldcx = cx;
	oldcy = cy;

	// DPad works in the map
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;
	if (wp->btns_h & WPAD_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_UP)
		J |= VBA_UP;
	if (wp->btns_h & WPAD_BUTTON_DOWN)
		J |= VBA_DOWN;

	// Pause
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Map (actually objectives)
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// talk or interact
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	// spells
	if (wp->btns_h & WPAD_BUTTON_B)
		J |= VBA_BUTTON_B;

	// Run (uses emulator speed button)
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_SPEED;

	// CAKTODO spell gestures

	return J;
}

u32 Mario1DXInput(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);

	bool run = false;


	run = (wp->exp.type==WPAD_EXP_NUNCHUK && wp->exp.nunchuk.js.mag> 0.83) || (wp->exp.type==WPAD_EXP_CLASSIC && wp->exp.classic.ljs.mag> 0.83);
	run = run && ((J & VBA_LEFT) || (J & VBA_RIGHT));

	if (wp->exp.type==WPAD_EXP_NUNCHUK)
	{
		// Pause
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;

		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Run, pick up
		if (wp->btns_h & WPAD_BUTTON_B || run)
			J |= VBA_BUTTON_B;
		// Starspin shoots when using fireflower
		if (fabs(wp->gforce.x)> 1.5)
			J |= VBA_BUTTON_B;

		// Camera (must come before Crouch)
		if (!(wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
			J &= ~(VBA_DOWN | VBA_UP);
		if (wp->btns_h & WPAD_BUTTON_UP)
			J |= VBA_UP;
		if (wp->btns_h & WPAD_BUTTON_DOWN)
			J |= VBA_DOWN;
		// Crouch (must come after Camera)
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z)
			J |= VBA_DOWN;

		// Speed
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
	}
	else if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		// Pause
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_X || wp->btns_h & WPAD_CLASSIC_BUTTON_Y || run)
			J |= VBA_BUTTON_B;
		// Spin attack
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Camera
		J &= ~(VBA_DOWN | VBA_UP);
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L)
			J |= VBA_DOWN;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R)
			J |= VBA_UP;
		// Crouch
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZL)
		{
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}
		// DPad movement/camera
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_UP)
			J |= VBA_UP;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_DOWN)
			J |= VBA_DOWN;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_LEFT)
			J |= VBA_LEFT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_RIGHT)
			J |= VBA_RIGHT;
		// Speed
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZR)
			J |= VBA_SPEED;
	}
	else
		J |= StandardSideways(pad);


	J |= StandardGamecube(pad) | StandardKeyboard(pad);

	return J;
}

u32 Mario1ClassicInput(unsigned short pad)
{
	u32 J = StandardMovement(pad) | StandardDPad(pad);
	WPADData * wp = WPAD_Data(pad);

	bool run = false;

	run = (wp->exp.type==WPAD_EXP_NUNCHUK && wp->exp.nunchuk.js.mag> 0.83) || (wp->exp.type==WPAD_EXP_CLASSIC && wp->exp.classic.ljs.mag> 0.83);
	run = run && ((J & VBA_LEFT) || (J & VBA_RIGHT));

	if (wp->exp.type==WPAD_EXP_NUNCHUK)
	{
		// Pause
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;

		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Run, pick up
		if (wp->btns_h & WPAD_BUTTON_B || run)
			J |= VBA_BUTTON_B;
		// Starspin shoots when using fireflower
		if (fabs(wp->gforce.x)> 1.5)
			J |= VBA_BUTTON_B;

		// Crouch
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z)
		{
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}

		// Speed
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
	}
	else if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		// DPad movement
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_UP)
			J |= VBA_UP;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_DOWN)
			J |= VBA_DOWN;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_LEFT)
			J |= VBA_LEFT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_RIGHT)
			J |= VBA_RIGHT;
		// Pause
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_X || wp->btns_h & WPAD_CLASSIC_BUTTON_Y || run)
			J |= VBA_BUTTON_B;
		// Spin attack
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Crouch
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZL || wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L)
		{
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}
		// Speed
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZR)
			J |= VBA_SPEED;
	}
	else
		J |= StandardSideways(pad);


	J |= StandardGamecube(pad) | StandardKeyboard(pad);

	return J;
}

u32 MarioLand1Input(unsigned short pad)
{
	u32 J = StandardMovement(pad) | StandardDPad(pad);
	WPADData * wp = WPAD_Data(pad);
	bool run = false;

	run = (wp->exp.type==WPAD_EXP_NUNCHUK && wp->exp.nunchuk.js.mag> 0.83) || (wp->exp.type==WPAD_EXP_CLASSIC && wp->exp.classic.ljs.mag> 0.83);
	run = run && ((J & VBA_LEFT) || (J & VBA_RIGHT));

	if (wp->exp.type == WPAD_EXP_NONE)
		J |= StandardSideways(pad);
	else if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		// DPad movement
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_UP)
			J |= VBA_UP;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_DOWN)
			J |= VBA_DOWN;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_LEFT)
			J |= VBA_LEFT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_RIGHT)
			J |= VBA_RIGHT;
		// Pause
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_X || wp->btns_h & WPAD_CLASSIC_BUTTON_Y || run)
			J |= VBA_BUTTON_B;
		// Spin attack
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Crouch
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZL || wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L)
		{
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}
		// Speed
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZR || wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R)
			J |= VBA_SPEED;
	}
	else if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		// Pause
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & WPAD_BUTTON_B || run)
			J |= VBA_BUTTON_B;
		if (fabs(wp->gforce.x)> 1.5)
			J |= VBA_BUTTON_B;
		// Crouch
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z)
			J |= VBA_DOWN;
		// Speed
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C || wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
	}
	else
		J |= StandardSideways(pad);
	// CAKTODO joystick run

	J |= StandardGamecube(pad) | StandardKeyboard(pad);

	return J;
}

u32 MarioLand2Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);
	bool run = false;

	run = (wp->exp.type==WPAD_EXP_NUNCHUK && wp->exp.nunchuk.js.mag> 0.83) || (wp->exp.type==WPAD_EXP_CLASSIC && wp->exp.classic.ljs.mag> 0.83);
	run = run && ((J & VBA_LEFT) || (J & VBA_RIGHT));

	if (wp->exp.type == WPAD_EXP_NONE)
	J |= StandardSideways(pad);
	else if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		// DPad movement
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_UP)
			J |= VBA_UP;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_DOWN)
			J |= VBA_DOWN;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_LEFT)
			J |= VBA_LEFT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_RIGHT)
			J |= VBA_RIGHT;
		// Pause
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_X || wp->btns_h & WPAD_CLASSIC_BUTTON_Y || run)
			J |= VBA_BUTTON_B;
		// Spin attack
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_A)
			J |= VBA_DOWN | VBA_BUTTON_A;
		// Camera
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L)
			J |= VBA_DOWN | VBA_BUTTON_B;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R)
			J |= VBA_UP | VBA_BUTTON_B;
		// Crouch
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZL && (!(wp->btns_h & WPAD_CLASSIC_BUTTON_A)))
		{
			J |= VBA_DOWN;
			// if the run button is held down, crouching also looks down
			// which we don't want when using the Z button
			J &= ~VBA_BUTTON_B;
			J &= ~VBA_UP;
		}
		// Speed
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZR)
			J |= VBA_SPEED;

	}
	else if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		// Pause
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & WPAD_BUTTON_B || run)
			J |= VBA_BUTTON_B;
		// Spin attack
		if (fabs(wp->gforce.x)> 1.4)
			J |= VBA_DOWN | VBA_BUTTON_A;
		// Camera
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C)
			J |= VBA_BUTTON_B;
		if (wp->btns_h & WPAD_BUTTON_UP)
			J |= VBA_BUTTON_B | VBA_UP;
		if (wp->btns_h & WPAD_BUTTON_DOWN)
			J |= VBA_BUTTON_B | VBA_DOWN;
		if (wp->btns_h & WPAD_BUTTON_LEFT)
			J |= VBA_BUTTON_B | VBA_LEFT;
		if (wp->btns_h & WPAD_BUTTON_RIGHT)
			J |= VBA_BUTTON_B | VBA_RIGHT;
		// Crouch
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z && (!(wp->btns_h & WPAD_BUTTON_A)))
		{
			J |= VBA_DOWN;
			// if the run button is held down, crouching also looks down
			// which we don't want when using the Z button
			J &= ~VBA_BUTTON_B;
			J &= ~VBA_UP;
		}
		// Speed
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
	}
	else
		J |= StandardSideways(pad);

	J |= StandardGamecube(pad) | StandardKeyboard(pad);

	return J;

}

u32 Mario2Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);

	// DPad looks around
	if (wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_R;
	if (wp->btns_h & WPAD_BUTTON_LEFT)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_UP)
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_DOWN)
		J |= VBA_BUTTON_L;

	// Pause
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Select
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// Jump
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	// Run, pick up
	if (wp->btns_h & WPAD_BUTTON_B)
		J |= VBA_BUTTON_B;
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_B;

	// Crouch
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
		J |= VBA_DOWN;

	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
		J |= VBA_BUTTON_L;
	if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_R;
	if (wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_BUTTON_R;

	return J;
}

u32 MarioWorldInput(unsigned short pad)
{
	u32 J = StandardMovement(pad) | StandardDPad(pad);

	//J &= ~(VBA_DOWN | VBA_UP);
	u32 jp = PAD_ButtonsHeld(pad);

	u8 CarryState = CPUReadByte(0x3003F06); // 01 = carrying, 00 = not carrying
	u8 InYoshisMouth = CPUReadByte(0x3003F53); // FF = nothing, 00 = no level, else thing
	u8 FallState = CPUReadByte(0x3003FA1); // 0B = jump, 24 = fall
	u8 RidingYoshi = CPUReadByte(0x3004302); // 00 = not riding, 01 = riding
	static bool NeedStomp = false;
	bool dontrun = false;
	bool run = false;
	static bool dontletgo = false;
	dontrun = (InYoshisMouth != 0 && InYoshisMouth != 0xFF && RidingYoshi);
	bool BButton = false;

	WPADData * wp = WPAD_Data(pad);

	run = (wp->exp.type==WPAD_EXP_NUNCHUK && wp->exp.nunchuk.js.mag> 0.83) || (wp->exp.type==WPAD_EXP_CLASSIC && wp->exp.classic.ljs.mag> 0.83);
	BButton = (wp->exp.type == WPAD_EXP_CLASSIC && (wp->btns_h & WPAD_CLASSIC_BUTTON_X || wp->btns_h & WPAD_CLASSIC_BUTTON_Y)) || wp->btns_h & WPAD_BUTTON_B;

	BButton = BButton || jp & PAD_BUTTON_B;
	if (run && CarryState && !BButton)
		dontletgo = true;
	if (!CarryState)
		dontletgo = false;

	// Pause
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Select
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// Jump
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	// Run, pick up
	if ((run && !dontrun) || wp->btns_h & WPAD_BUTTON_B || dontletgo)
		J |= VBA_BUTTON_B;
	if (wp->btns_h & WPAD_BUTTON_B && dontletgo)
	{
		J = J & ~VBA_BUTTON_B;
		dontletgo = false;
	}
	// Spin attack
	if (fabs(wp->gforce.x)> 1.5)
		J |= VBA_BUTTON_R;

	// Butt stomp
	if (FallState!=0 && (wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) && (!RidingYoshi))
		NeedStomp = true;
	if (FallState!=0 && (wp->exp.type == WPAD_EXP_CLASSIC) && (wp->btns_h & WPAD_CLASSIC_BUTTON_ZL || wp->btns_h & WPAD_CLASSIC_BUTTON_ZR) && (!RidingYoshi))
		NeedStomp = true;
	// Crouch
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
	{
		J |= VBA_DOWN; // Crouch
		J &= ~VBA_UP;
	}

	// Camera
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
	{
		if (J & VBA_DOWN || J & VBA_UP)
			J |= VBA_BUTTON_L;
		else if (J & VBA_RIGHT || J & VBA_LEFT)
		{
			J |= VBA_BUTTON_L;
			J &= ~VBA_RIGHT;
			J &= ~VBA_LEFT;
		}
	}

	if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_X || wp->btns_h & WPAD_CLASSIC_BUTTON_Y)
			J |= VBA_BUTTON_B;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_A || wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L)
			J |= VBA_BUTTON_L;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZL || wp->btns_h & WPAD_CLASSIC_BUTTON_ZR)
		{
			J |= VBA_DOWN; // Crouch
			J &= ~VBA_UP;
		}
	}

	BButton = (wp->exp.type == WPAD_EXP_CLASSIC && (wp->btns_h & WPAD_CLASSIC_BUTTON_X || wp->btns_h & WPAD_CLASSIC_BUTTON_Y)) || wp->btns_h & WPAD_BUTTON_B;

	if (jp & PAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	if (jp & PAD_BUTTON_B)
		J |= VBA_BUTTON_B;
	if (jp & PAD_BUTTON_X)
		J |= VBA_BUTTON_R;
	if (jp & PAD_BUTTON_Y)
		J |= VBA_BUTTON_SELECT;
	if (jp & PAD_TRIGGER_R)
		J |= VBA_BUTTON_R;
	if (jp & PAD_TRIGGER_L)
		J |= VBA_BUTTON_L;
	if (jp & PAD_BUTTON_START)
		J |= VBA_BUTTON_START;
	if (jp & PAD_TRIGGER_Z)
	{
		J |= VBA_DOWN;
		J &= ~VBA_UP;
	}
	// if we try to use Yoshi's tongue while running, release run for one frame
	if (run && BButton && !dontrun && !dontletgo)
	{
		J &= ~VBA_BUTTON_B;
	}
	if (NeedStomp && FallState == 0 && !RidingYoshi)
	{
		J |= VBA_BUTTON_R; // spin attack only works when on ground
		NeedStomp = false;
	}

	J |= StandardKeyboard(pad);

	return J;
}

u32 Mario3Input(unsigned short pad)
{
	u32 J = StandardMovement(pad) | StandardDPad(pad);
	WPADData * wp = WPAD_Data(pad);
	bool run = false;

	run = (wp->exp.type==WPAD_EXP_NUNCHUK && wp->exp.nunchuk.js.mag> 0.83) || (wp->exp.type==WPAD_EXP_CLASSIC && wp->exp.classic.ljs.mag> 0.83);
	run = run && ((J & VBA_LEFT) || (J & VBA_RIGHT));

	if (wp->exp.type == WPAD_EXP_NONE)
		J |= StandardSideways(pad);
	else if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		// DPad movement
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_UP)
			J |= VBA_UP;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_DOWN)
			J |= VBA_DOWN;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_LEFT)
			J |= VBA_LEFT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_RIGHT)
			J |= VBA_RIGHT;
		// Pause
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_L;
		// Replay menu
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_X || wp->btns_h & WPAD_CLASSIC_BUTTON_Y || run)
			J |= VBA_BUTTON_B;
		// Spin attack (racoon costume, fire flower)
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Crouch
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZL || wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L)
		{
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}
		// Speed
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZR)
			J |= VBA_SPEED;

	}
	else if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		// Pause
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_L;
		// Replay Menu
		if (wp->btns_h & WPAD_BUTTON_1)
			J |= VBA_BUTTON_SELECT;
		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Run, pick up, throw, fire
		if (wp->btns_h & WPAD_BUTTON_B || run)
			J |= VBA_BUTTON_B;
		// Spin attack (racoon, fire flower)
		if (fabs(wp->gforce.x)> 1.4)
			J |= VBA_BUTTON_B;
		// Crouch
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z)
		{
			J |= VBA_DOWN;
			J &= ~VBA_UP;
		}
		// Speed
		if (wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
	}
	else
		J |= StandardSideways(pad);

	J |= StandardGamecube(pad) | StandardKeyboard(pad);

	return J;
}

u32 YoshiIslandInput(unsigned short pad)
{
	u32 J = StandardMovement(pad) | StandardDPad(pad);

	u32 jp = PAD_ButtonsHeld(pad);

	//static bool NeedStomp = false;
	//bool dontrun = false;
	//bool run = false;
	//static bool dontletgo = false;
	//bool BButton = false;

	WPADData * wp = WPAD_Data(pad);
	//run = (wp->exp.type==WPAD_EXP_NUNCHUK && wp->exp.nunchuk.js.mag > 0.83) || (wp->exp.type==WPAD_EXP_CLASSIC && wp->exp.classic.ljs.mag > 0.83);
	//BButton = (wp->exp.type == WPAD_EXP_CLASSIC && (wp->btns_h & WPAD_CLASSIC_BUTTON_X || wp->btns_h & WPAD_CLASSIC_BUTTON_Y)) || wp->btns_h & WPAD_BUTTON_B;

	//BButton = BButton || jp & PAD_BUTTON_B;


	/*** Report wp->btns_h buttons (gamepads) ***/


	// Pause
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	// Select
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	// Jump
	if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	// Run, pick up
	if (wp->btns_h & WPAD_BUTTON_B)
		J |= VBA_BUTTON_B;
	// throw
	if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_BUTTON_R;

	// Butt stomp
	//if (FallState!=0 && (wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) && (!RidingYoshi))
	//	NeedStomp = true;
	//if (FallState!=0 && (wp->exp.type == WPAD_EXP_CLASSIC) && (wp->btns_h & WPAD_CLASSIC_BUTTON_ZL || wp->btns_h & WPAD_CLASSIC_BUTTON_ZR) && (!RidingYoshi))
	//	NeedStomp = true;

	// Crouch, lay egg
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z))
	{
		J |= VBA_DOWN; // Crouch
		J &= ~VBA_UP;
	}

	// Camera
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_h & WPAD_NUNCHUK_BUTTON_C))
	{
		J |= VBA_BUTTON_L;
	}

	if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_X)
			J |= VBA_BUTTON_B;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_Y)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_B)
			J |= VBA_BUTTON_A;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_A || wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L)
			J |= VBA_BUTTON_L;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZL || wp->btns_h & WPAD_CLASSIC_BUTTON_ZR)
		{
			J |= VBA_DOWN; // Crouch
			J &= ~VBA_UP;
		}
	}

	if (jp & PAD_BUTTON_A)
		J |= VBA_BUTTON_A;
	if (jp & PAD_BUTTON_B)
		J |= VBA_BUTTON_B;
	if (jp & PAD_BUTTON_X)
		J |= VBA_BUTTON_R;
	if (jp & PAD_BUTTON_Y)
		J |= VBA_BUTTON_SELECT;
	if (jp & PAD_TRIGGER_R)
		J |= VBA_BUTTON_R;
	if (jp & PAD_TRIGGER_L)
		J |= VBA_BUTTON_L;
	if (jp & PAD_BUTTON_START)
		J |= VBA_BUTTON_START;
	if (jp & PAD_TRIGGER_Z)
	{
		J |= VBA_DOWN;
		J &= ~VBA_UP;
	}
	//if (NeedStomp && FallState==0 && !RidingYoshi) {
	//	J |= VBA_BUTTON_R; // spin attack only works when on ground
	//	NeedStomp = false;
	//}

	return J;
}

u32 UniversalGravitationInput(unsigned short pad)
{
	TiltScreen = true;
	u32 J = StandardMovement(pad);

	//u32 jp = PAD_ButtonsHeld(pad);

	//static bool NeedStomp = false;
	//bool dontrun = false;
	//bool run = false;
	//static bool dontletgo = false;
	//bool BButton = false;

	WPADData * wp = WPAD_Data(pad);
	//run = (wp->exp.type==WPAD_EXP_NUNCHUK && wp->exp.nunchuk.js.mag > 0.83) || (wp->exp.type==WPAD_EXP_CLASSIC && wp->exp.classic.ljs.mag > 0.83);

	if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		TiltSideways = false;
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select, L & R do nothing!
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & WPAD_BUTTON_1)
			J |= VBA_BUTTON_L | VBA_SPEED;
		if (wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_BUTTON_R | VBA_SPEED;

		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// tongue
		if (wp->btns_h & WPAD_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Crouch, butt stomp
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z)
		{
			J |= VBA_DOWN; // Crouch
			J &= ~VBA_UP;
		}
		// Camera
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C)
		{
			J |= VBA_UP;
		}
	}
	else if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		TiltSideways = false;
		J |= StandardDPad(pad);
		// A bit stupid playing with the classic controller, but nevermind.
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_X)
			J |= VBA_BUTTON_B;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_Y)
			J |= VBA_BUTTON_R;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_B || wp->btns_h & WPAD_CLASSIC_BUTTON_A)
			J |= VBA_BUTTON_A;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_L || wp->btns_h & WPAD_CLASSIC_BUTTON_ZL)
			J |= VBA_DOWN;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R)
			J |= VBA_UP;
		if (WPAD_CLASSIC_BUTTON_ZR)
			J |= VBA_SPEED;
	}
	else
	{
		TiltSideways = true;
		J |= StandardSideways(pad);
	}

	// A bit stupid playing with keyboard or gamecube pad when you need to tilt the wiimote...
	J |= StandardKeyboard(pad) | StandardGamecube(pad);

	return J;
}

u32 TwistedInput(unsigned short pad)
{
	// Change this to true if you want to see the screen tilt.
	TiltScreen = false;
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);

	if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		TiltSideways = false;
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select and L do nothing!
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & WPAD_BUTTON_1)
			J |= VBA_BUTTON_L | VBA_SPEED;
		if (wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_BUTTON_L | VBA_SPEED;

		// A Button
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// B Button
		if (wp->btns_h & WPAD_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Grab an icon and prevent menu from spinning
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z)
			J |= VBA_BUTTON_R;
		// Speed
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C)
			J |= VBA_SPEED;
	}
	else if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		TiltSideways = false;
		J |= StandardDPad(pad) | StandardClassic(pad);

	}
	else
	{
		TiltSideways = true;
		J |= StandardSideways(pad);
		if (wp->btns_h & WPAD_BUTTON_B)
			J |= VBA_SPEED;
	}

	// A bit stupid playing with keyboard or gamecube pad when you need to tilt the wiimote...
	J |= StandardKeyboard(pad) | StandardGamecube(pad);

	return J;
}

u32 KirbyTntInput(unsigned short pad)
{
	TiltScreen = false;
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);

	if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		TiltSideways = false;
		J |= StandardDPad(pad);
		// Pause
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		// Select and L do nothing!
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		if (wp->btns_h & WPAD_BUTTON_1)
			J |= VBA_BUTTON_B;
		if (wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_BUTTON_A;

		// A Button
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// B Button
		if (wp->btns_h & WPAD_BUTTON_B)
			J |= VBA_BUTTON_B;
		// Speed
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C)
			J |= VBA_SPEED;
	}
	else if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		TiltSideways = false;
		J |= StandardDPad(pad) | StandardClassic(pad);
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;

	}
	else
	{
		TiltSideways = true;
		J |= StandardSideways(pad);
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		if (wp->btns_h & WPAD_BUTTON_B)
			J |= VBA_SPEED;
	}

	// A bit stupid playing with keyboard or gamecube pad when you need to tilt the wiimote...
	J |= StandardKeyboard(pad) | StandardGamecube(pad);

	return J;
}

u32 MohInfiltratorInput(unsigned short pad)
{
	u32 J = StandardMovement(pad);


	WPADData * wp = WPAD_Data(pad);

	if (wp->exp.type == WPAD_EXP_NONE)
	J |= StandardSideways(pad);
	else if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		J |= StandardClassic(pad);
	}
	else if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		// Pause, objectives, menu
		if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
		// Action button, use
		if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_L;
		// Use sights/scope, not needed in this game
		if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A | VBA_BUTTON_L;
		// Shoot
		if (wp->btns_h & WPAD_BUTTON_B)
		J |= VBA_BUTTON_A;
		// Reload
		if (fabs(wp->gforce.y)> 1.6 || wp->btns_h & WPAD_BUTTON_UP || wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_BUTTON_L;
		// Strafe
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C)
		J |= VBA_BUTTON_R;
		// Change weapon
		if (wp->btns_h & WPAD_BUTTON_LEFT || wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_B;
		// Speed
		if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_SPEED;
	}
	else
	J |= StandardSideways(pad);

	J |= StandardGamecube(pad) | StandardKeyboard(pad);

	return J;
}

u32 MohUndergroundInput(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);
	static bool crouched = false;

	if (wp->exp.type == WPAD_EXP_NONE)
	{
		J |= StandardSideways(pad);
		CursorVisible = false;
	}
	else if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		J |= StandardClassic(pad);
		CursorVisible = false;
	}
	else if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		// Joystick controls L and R, not left and right
		if (J & VBA_LEFT)
		J |= VBA_BUTTON_L;
		if (J & VBA_RIGHT)
		J |= VBA_BUTTON_R;
		J &= ~(VBA_LEFT | VBA_RIGHT);
		// Cursor controls left and right for turning
		CursorVisible = true;
		if (CursorValid)
		{
			if (CursorX < 320 - 40)
			J |= VBA_LEFT;
			else if (CursorX> 320 + 40)
			J |= VBA_RIGHT;
		}
		// Crouch
		if (wp->btns_h & WPAD_BUTTON_DOWN)
		crouched = !crouched;

		// Pause, objectives, menu
		if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
		// Action button, not needed in this game
		if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= 0;
		// Use sights/scope, not needed in this game
		if (wp->btns_h & WPAD_BUTTON_A)
		J |= VBA_BUTTON_A;
		// Shoot
		if (wp->btns_h & WPAD_BUTTON_B)
		J |= VBA_BUTTON_A;
		// Reload
		if (fabs(wp->gforce.y)> 1.6 || wp->btns_h & WPAD_BUTTON_UP || wp->btns_h & WPAD_BUTTON_2)
		J |= VBA_BUTTON_SELECT;
		// Change weapon
		if (wp->btns_h & WPAD_BUTTON_LEFT || wp->btns_h & WPAD_BUTTON_RIGHT)
		J |= VBA_BUTTON_B;
		// Speed
		if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_SPEED;
	}
	else
	{
		J |= StandardSideways(pad);
		CursorVisible = false;
	}

	J |= StandardGamecube(pad) | StandardKeyboard(pad);
	if (crouched && (!(J & VBA_BUTTON_L)) && (!(J & VBA_BUTTON_R)))
		J |= VBA_BUTTON_L | VBA_BUTTON_R;

	return J;
}

u32 BoktaiInput(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);
	static bool GunRaised = false;

	J |= StandardDPad(pad);
	// Action
	if (wp->btns_h & WPAD_BUTTON_A)
	J |= VBA_BUTTON_A;
	// Hold Gun Del Sol up to the light to recharge
	if ((-wp->orient.pitch)> 45)
	{
		GunRaised = true;
	}
	else if ((-wp->orient.pitch) < 40)
	{
		GunRaised = false;
	}
	if (GunRaised)
	J |= VBA_BUTTON_A;
	// Fire Gun Del Sol
	if (wp->btns_h & WPAD_BUTTON_B)
	J |= VBA_BUTTON_B;
	// Look around or change subscreen
	if (wp->btns_h & WPAD_BUTTON_2)
	J |= VBA_BUTTON_R;
	// Start
	if (wp->btns_h & WPAD_BUTTON_PLUS)
	J |= VBA_BUTTON_START;
	// Select
	if (wp->btns_h & WPAD_BUTTON_MINUS)
	J |= VBA_BUTTON_SELECT;

	if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		J |= StandardClassic(pad);
	}
	else if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		// Look around
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C)
		J |= VBA_BUTTON_R;
		// Change element or change subscreen
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z)
		J |= VBA_BUTTON_L;

		if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_SPEED;
	}
	else
	{
		// Change element or change subscreen
		if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_L;
	}

	J |= StandardGamecube(pad) | StandardKeyboard(pad);

	return J;
}

u32 Boktai2Input(unsigned short pad)
{
	u32 J = StandardMovement(pad);
	WPADData * wp = WPAD_Data(pad);
	static bool GunRaised = false;

	J |= StandardDPad(pad);
	// Action
	if (wp->btns_h & WPAD_BUTTON_A)
	J |= VBA_BUTTON_A;
	// Hold gun or hand up to the light to recharge
	if ((-wp->orient.pitch)> 45)
	{
		GunRaised = true;
	}
	else if ((-wp->orient.pitch) < 40)
	{
		GunRaised = false;
	}
	if (GunRaised)
	J |= VBA_BUTTON_A;
	// Fire Gun Del Sol
	if (wp->btns_h & WPAD_BUTTON_B)
	J |= VBA_BUTTON_B;
	// Swing sword or hammer or stab spear
	if (fabs(wp->gforce.x)> 1.8)
	J |= VBA_BUTTON_B;
	// Look around or change subscreen
	if (wp->btns_h & WPAD_BUTTON_2)
	J |= VBA_BUTTON_R;
	// Start
	if (wp->btns_h & WPAD_BUTTON_PLUS)
	J |= VBA_BUTTON_START;
	// Select
	if (wp->btns_h & WPAD_BUTTON_MINUS)
	J |= VBA_BUTTON_SELECT;

	if (wp->exp.type == WPAD_EXP_CLASSIC)
	{
		J |= StandardClassic(pad);
	}
	else if (wp->exp.type == WPAD_EXP_NUNCHUK)
	{
		// Look around
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C)
		J |= VBA_BUTTON_R;
		// Change element or change subscreen
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z)
		J |= VBA_BUTTON_L;

		if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_SPEED;
	}
	else
	{
		// Change element or change subscreen
		if (wp->btns_h & WPAD_BUTTON_1)
		J |= VBA_BUTTON_L;
	}

	J |= StandardGamecube(pad) | StandardKeyboard(pad);

	return J;
}

#endif

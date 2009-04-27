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
#include "gb/gbGlobals.h"

void DebugPrintf(const char *format, ...);
void gbSetSpritePal(u8 WhichPal, u32 bright, u32 medium, u32 dark);

#define MK1_CAGE 0
#define MK1_KANO 1
#define MK1_RAIDEN 2
#define MK1_LIUKANG 3
#define MK1_SCORPION 4
#define MK1_SUBZERO 5
#define MK1_SONYA 6
#define MK1_GORO 7
#define MK1_SHANGTSUNG 8

#define MK2_LIUKANG 0
#define MK2_SUBZERO 1
#define MK2_KITANA 2
#define MK2_REPTILE 3
#define MK2_SHANGTSUNG 4
#define MK2_MILEENA 5
#define MK2_SCORPION 6
#define MK2_JAX 7
// boss and secret characters
#define MK2_KINTARO 8
#define MK2_KHAN 9
#define MK2_SMOKE 10
#define MK2_JADE 11

#define MK3_SINDEL 0
#define MK3_SEKTOR 1
#define MK3_KABAL 2
#define MK3_SHEEVA 3 
#define MK3_SMOKE 4 
#define MK3_SUBZERO 5 
#define MK3_KANO 6 
#define MK3_SONYA 7 
#define MK3_CYRAX 8
// boss
#define MK3_SHAOKHAN 9
#define MK3_RAND 10

#define MK4_RAIDEN 0
#define MK4_QUANCHI 1
#define MK4_FUJIN 2
#define MK4_LIUKANG 3
#define MK4_REPTILE 4
#define MK4_SUBZERO 5
#define MK4_REIKO 6
#define MK4_TANYA 7
#define MK4_SCORPION 8
// boss
#define MK4_SHINNOK 9

#define MKA_Rain 0
#define MKA_Reptile 1
#define MKA_Stryker 2
#define MKA_Jax 3
#define MKA_Nightwolf 4
#define MKA_Jade 5
#define MKA_NoobSaibot 6
#define MKA_Sonya 7
#define MKA_Kano 8
#define MKA_Mileena 9
#define MKA_Ermac 10
#define MKA_SubZero 11
#define MKA_SubZero2 12
#define MKA_KungLao 13
#define MKA_Sektor 14
#define MKA_Kitana 15
#define MKA_Mystery 16
#define MKA_Scorpion 17
#define MKA_Cyrax 18
#define MKA_Kabal 19
#define MKA_Sindel 20
#define MKA_Smoke 21
#define MKA_LiuKang 22
#define MKA_ShangTsung 23
#define MKA_Motaro 24
#define MKA_ShaoKhan 25

static bool HP=0,LP=0,HK=0,LK=0,BL=0,Throw=0,CS=0,F=0,B=0,Select=0,Start=0,SpecialMove=0;
static u16	OurHealth=0,OpponentHealth=0,OurOldHealth=0;
static s16  OurX=0,OpponentX=1;
static u32  VBA_FORWARD=VBA_RIGHT, VBA_BACK=VBA_LEFT;

static int ChameleonChangeTime = 0;

u32 GetMKInput(unsigned short pad, int rumbleTime=4) {
	u32 J = StandardMovement(pad) | DecodeKeyboard(pad) | DecodeWiimote(pad);
    HP=0;LP=0;HK=0;LK=0;BL=0;Throw=0;CS=0;F=0;B=0;Select=0;Start=0;SpecialMove=0;

	// Rumble when they lose health!
	if (OurHealth < OurOldHealth)
		systemGameRumble(rumbleTime);
	OurOldHealth = OurHealth;
		
#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		// Punch
		if (wp->btns_h & WPAD_BUTTON_LEFT) LP = true;
		if (wp->btns_h & WPAD_BUTTON_UP) HP = true;
		// Kick
		if (wp->btns_h & WPAD_BUTTON_DOWN) LK = true;
		if (wp->btns_h & WPAD_BUTTON_RIGHT) HK = true;
		// Block
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) BL = true;
		// Throw
		if (wp->btns_h & WPAD_BUTTON_A) Throw = true;
		// Run / Change Styles
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C) CS = true;
		// Start and Select
		if (wp->btns_h & WPAD_BUTTON_MINUS) Select=true;
		if (wp->btns_h & WPAD_BUTTON_PLUS) Start=true;
		// Special move
		if (wp->btns_h & WPAD_BUTTON_B) SpecialMove=true;
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		// D-Pad
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_UP) J |= VBA_UP;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_DOWN) J |= VBA_DOWN;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_LEFT) J |= VBA_LEFT;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_RIGHT) J |= VBA_RIGHT;
		// Punch
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_Y) LP = true;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_X) HP = true;
		// Kick
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_B) LK = true;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_A) HK = true;
		// Throw
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_ZR) Throw = true;
		// Block
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_FULL_R) BL = true; // block
		// Run / Change Styles
		if (wp->btns_h & (WPAD_CLASSIC_BUTTON_FULL_L | WPAD_CLASSIC_BUTTON_ZL)) CS = true;
		// Start and Select
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_MINUS) Select = true;
		if (wp->btns_h & WPAD_CLASSIC_BUTTON_PLUS) Start = true;
		// Special move
		if (wp->btns_h & WPAD_BUTTON_B) SpecialMove=true;
	}
#endif
	{
		u32 gc = PAD_ButtonsHeld(pad);
		// DPad moves
		if (gc & PAD_BUTTON_UP) J |= VBA_UP;
		if (gc & PAD_BUTTON_DOWN) J |= VBA_DOWN;
		if (gc & PAD_BUTTON_LEFT) J |= VBA_LEFT;
		if (gc & PAD_BUTTON_RIGHT) J |= VBA_RIGHT;
		// Start and Select
		if (gc & PAD_BUTTON_START) Start = true;
		// Punch
		if (gc & PAD_BUTTON_B) LP = true;
		if (gc & PAD_BUTTON_Y) HP = true;
		// Kick
		if (gc & PAD_BUTTON_A) LK = true;
		if (gc & PAD_BUTTON_X) HK = true;
		// Block
		if (gc & PAD_TRIGGER_R) BL = true;
		// Run / Change Style
		if (gc & PAD_TRIGGER_L) CS = true;
		// Throw
		if (gc & PAD_TRIGGER_Z) Throw = true;
	}
	// Check which side they are on
	if (OurX < OpponentX) {
		VBA_FORWARD = VBA_RIGHT;
		VBA_BACK = VBA_LEFT;
	} else {
		VBA_FORWARD = VBA_LEFT;
		VBA_BACK = VBA_RIGHT;
	}
	if (J & VBA_FORWARD) F = true;
	if (J & VBA_BACK) B = true;
	return J;
}

// Allows writes to the ROM memory for hacking
void gbaWriteMemory(u32 addr, u32 value) {
	if (addr<0x8000000 || addr>=0xD000000) CPUWriteMemory(addr, value);
#ifndef USE_VM
	else WRITE32LE(((u16 *)&rom[addr&0x1FFFFFC]), value);
#endif
}
void gbaWriteHalfWord(u32 addr, u16 value) {
	if (addr<0x8000000 || addr>=0xD000000) CPUWriteHalfWord(addr, value);
#ifndef USE_VM
	else WRITE16LE(((u16 *)&rom[addr&0x1FFFFFC]), value);
#endif
}
void gbaWriteByte(u32 addr, u8 value) {
	if (addr<0x8000000 || addr>=0xD000000) CPUWriteByte(addr, value);
#ifndef USE_VM
	else rom[addr & 0x1FFFFFF] = value;
#endif
}
void gbWriteByte(u16 addr, u8 value) {
	if (addr>=0x8000) gbWriteMemory(addr, value);
	else gbRom[addr] = value;
}

u32 MK1Input(unsigned short pad) {
	OurHealth = gbReadMemory(0xD695);
	OpponentHealth = gbReadMemory(0xD696);
	OurX = gbReadMemory(0xCF00);
	OpponentX = gbReadMemory(0xCF26);
	u8 OurChar = gbReadMemory(0xD685);
	u8 OpponentChar = gbReadMemory(0xD686);
	u8 MenuChar = gbReadMemory(0xD61D)+1;
	static u8 OldMenuChar = 0;
	// Rumble when they change character
	if (MenuChar != OldMenuChar) {
		systemGameRumble(4);
		OldMenuChar = MenuChar;
	}
	u32 J = GetMKInput(pad, 8);
	if (LK || HK || BL) J |= VBA_BUTTON_A;
	if (LP || HP || BL) J |= VBA_BUTTON_B;
	if (Start) J |= VBA_BUTTON_START;
	if (Select) J |= VBA_BUTTON_SELECT;
	// Fix kick controls to what they should be!
	if (!(J & (VBA_UP | VBA_DOWN))) {
		if (B && HK) { // Make B+HK do roundhouse, while B+LK does sweep!
			J &= ~VBA_BACK;
			J |= VBA_FORWARD;
		} else if (F && HK) { // Make F+HK do normal high kick
			J &= ~VBA_FORWARD;
		} else if (F && LK) { // Make F+LK also do normal high kick, since no low kicks
			J &= ~VBA_FORWARD;
		}
	}
	if (Throw) J |= VBA_FORWARD | VBA_BUTTON_B;
	if (CS) J |= VBA_SPEED;
	return J;
}

u32 MK2Input(unsigned short pad) {
	u8 OurChar = gbReadMemory(0xDD01);
	u8 OpponentChar = gbReadMemory(0xDD01+0x40);
	OurX = gbReadMemory(0xDD12) | (gbReadMemory(0xDD13) << 8);
	OpponentX = gbReadMemory(0xDD12+0x40) | (gbReadMemory(0xDD13+0x40) << 8);
	OurHealth = gbReadMemory(0xDD20);
	OpponentHealth = gbReadMemory(0xDD20+0x40);

	u32 J = GetMKInput(pad, 5);
	if (LK || HK) J |= VBA_BUTTON_A;
	if (LP || HP) J |= VBA_BUTTON_B;
	if (BL) J |= VBA_BUTTON_START;
	if (Start || Select) J |= VBA_BUTTON_SELECT;
	// Can't fix kick controls to what they should be! Sorry
	if (Throw) J |= VBA_FORWARD | VBA_BUTTON_B;
	if (CS) J |= VBA_SPEED;
	return J;
}

u32 MK12Input(unsigned short pad) {
	OurHealth = 0;
	OpponentHealth = 0;
	OurX = 0;
	OpponentX = 1;

	u32 J = GetMKInput(pad, 5);
	if (LK || HK) J |= VBA_BUTTON_A;
	if (LP || HP) J |= VBA_BUTTON_B;
	if (BL) J |= VBA_BUTTON_START;
	// Fix kick controls to what they should be!
	if (!(J & (VBA_UP | VBA_DOWN))) {
		if (B && HK) { // Make B+HK do roundhouse, while B+LK does sweep!
			J &= ~VBA_BACK;
			J |= VBA_FORWARD;
		} else if (F && HK) { // Make F+HK do normal high kick
			J &= ~VBA_FORWARD;
		} else if (F && LK) { // Make F+LK also do normal high kick, since no low kicks
			J &= ~VBA_FORWARD;
		}
	}
	if (Throw) J |= VBA_FORWARD | VBA_BUTTON_B;
	if (Start || Select) J |= VBA_BUTTON_SELECT;
	if (CS) J |= VBA_SPEED;
	return J;
}

void MK3SetPal(int player, u8 NewChar, u8 SubChar=0) {
	switch (NewChar) {
		case MK3_SHEEVA: 
			gbSetSpritePal(player, 0xF5CCAC,0x9A7057,0x800000); 
			break;
		case MK3_KANO: 
			gbSetSpritePal(player, 0xA87860,0x882020,0x000000);
			break;
		case MK3_SINDEL: 
			gbSetSpritePal(player, 0xB8B8B8,0x7F5644,0xA818F0);
			break;
		case MK3_SUBZERO:
			if (SubChar==2) gbSetSpritePal(player, 0x101010,0x080808,0x000000); // noob saibot
			else if (SubChar==4) gbSetSpritePal(player, 0xB39890,0x909090,0x000000); // Chameleon
			else gbSetSpritePal(player, 0xB39890,0x707BFF,0x000020); // sub zero or frost
			break;
		case MK3_SMOKE: 
			if (SubChar==2) gbSetSpritePal(player, 0xB3A080,0x50A050,0x003000); // Reptile
			else gbSetSpritePal(player, 0xFFFFFF,0xA0A0A0,0x636363); // Smoke
			break;
		case MK3_CYRAX: 
			gbSetSpritePal(player, 0xC0C0B0,0x90A000,0x303800); 
			break;
		case MK3_SEKTOR: 
			gbSetSpritePal(player, 0xA09090,0xC00000,0x300000); 
			break;
		case MK3_SONYA: 
			if (SubChar==1) gbSetSpritePal(player, 0xC6A040,0x909090,0x000000); // Khameleon
			else gbSetSpritePal(player, 0xC6A040,0x96964D,0x000000); // Sonya
			break;
		case MK3_KABAL: 
			gbSetSpritePal(player, 0xB6B6B6,0x866232,0x1A1211); 
			break;
		case MK3_SHAOKHAN: 
			if (SubChar==1) gbSetSpritePal(player, 0xB3A080,0xFF1070,0x200008); // Reiko
			else gbSetSpritePal(player, 0xC0C0C0,0x7F5644,0x700000); // Shao Khan
			break;
	}
	return;
}


// Name must be 8 chars or less (preferably 7), fullname must be 9 chars or less
void MK3Rename(int n, const char *name, const char *fullname = NULL) {
	if (n<0 || n>10) return;
	// Rename the Player 1 health bar
	u16 addr = 0x2DD9 + n*8;
	int i;
	for (i=0; i<8; i++) {
		if (name[i]=='\0') {
			gbRom[addr+i] = '*';
			i++;
			while (i<8) {
				gbRom[addr+i] = ' ';
				i++;
			}
			break;
		} else gbRom[addr+i] = name[i];
	}
	// Rename the player wins message which is right aligned
	if (n>9) return;
	if (!fullname) fullname = name;
	addr = 0x2E9F + n*9;
	int len = strlen(fullname);
	if (len>9) len=9;
	int j = len-1;
	for (i=8; i>=0; i--) {
		if (j<0) gbRom[addr+i]=' ';
		else gbRom[addr+i]=fullname[j];
		j--;
	}
}

void MK3RenameEveryoneProperlyExcept(u8 n) {
	const char *names[MK3_SHAOKHAN+1] = {
		"SINDEL", "SEKTOR", "KABAL", "SHEEVA", "SMOKE", "SUBZERO", "KANO", "SONYA", "CYRAX", "SHAO"};
	const char *longnames[MK3_SHAOKHAN+1] = {
		NULL, NULL, NULL, NULL, NULL, "SUB-ZERO", NULL, NULL, NULL, "SHAO KHAN"};
	for (int i=0; i<=MK3_SHAOKHAN; i++) {
		if (i!=n) MK3Rename(i, names[i], longnames[i]);
	}
}

void MK3RandomNinja() {
	if (ChameleonChangeTime>0) {
		ChameleonChangeTime--;
		return;
	} else ChameleonChangeTime = 80;
	switch (rand() % 4) {
		case 0: // Sub-Zero
			MK3SetPal(1, MK3_SUBZERO);
			gbWriteMemory(0xCD00,MK3_SUBZERO); // use moves from...
			break;
		case 1: // Smoke
			MK3SetPal(1, MK3_SMOKE);
			gbWriteMemory(0xCD00,MK3_SMOKE); // use moves from...
			break;
		case 2: // Sektor
			MK3SetPal(1, MK3_SEKTOR, 1);
			gbWriteMemory(0xCD00,MK3_SEKTOR); // use moves from...
			break;
		case 3: // Cyrax
			MK3SetPal(1, MK3_CYRAX, 1);
			gbWriteMemory(0xCD00,MK3_CYRAX); // use moves from...
			break;
	}
}

void MK3RandomFemale() {
	if (ChameleonChangeTime>0) {
		ChameleonChangeTime--;
		return;
	} else ChameleonChangeTime = 80;
	switch (rand() % 3) {
		case 0: // Sonya
			MK3SetPal(1, MK3_SONYA);
			gbWriteMemory(0xCD00,MK3_SONYA); // use moves from...
			break;
		case 1: // Sindel
			MK3SetPal(1, MK3_SINDEL);
			gbWriteMemory(0xCD00,MK3_SINDEL); // use moves from...
			break;
		case 2: // Sheeva
			MK3SetPal(1, MK3_SHEEVA);
			gbWriteMemory(0xCD00,MK3_SHEEVA); // use moves from...
			break;
	}
}

void MK3Impersonate(u8 appearance, u8 moves, const char *name, const char *longname = NULL) {
	gbWriteMemory(0xC0F0, appearance);
	MK3Rename(appearance, name, longname);
	if (moves!=MK3_RAND)
		gbWriteMemory(0xCD00, moves);
}


u8 MK3SetSubchar(int Char, int Subchar, bool menu=false) {
	switch (Char) {
		case MK3_SUBZERO:
			if (Subchar>=5) Subchar = 0;
			switch (Subchar) {
			    // Subzero Unmasked
				case 0: 
					MK3Impersonate(MK3_SUBZERO, MK3_SUBZERO, "SUBZERO", "SUB-ZERO"); 
					break;
				// Cyborg
				case 1: 
					MK3Impersonate(MK3_SEKTOR, MK3_SUBZERO, "SUBZERO", "SUB-ZERO"); 
					break;
				// Noob Saibot
				case 2: 
					MK3Impersonate(MK3_SUBZERO, MK3_SUBZERO, "NOOB", "N. SAIBOT"); 
					break;
				// Frost
				case 3: 
					MK3Impersonate(MK3_SINDEL, MK3_SUBZERO, "FROST");
					break;
				// Chameleon
				case 4: 
					MK3Impersonate(MK3_SUBZERO, MK3_RAND, "CAMELEON", "CHAMELEON");
					break;
			}
			break;
		case MK3_SONYA:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
			    // Sonya Blade
				case 0: 
					MK3Impersonate(MK3_SONYA, MK3_SONYA, "SONYA");
					break;
				// Khameleon
				case 1: 
					MK3Impersonate(MK3_SONYA, MK3_RAND, "KAMELEON", "KHAMELEON");
					break;
			}
			break;
		case MK3_KANO:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
			    // Kano
				case 0: 
					MK3Impersonate(MK3_KANO, MK3_KANO, "KANO");
					break;
				// Cyborg Kano
				case 1: 
					MK3Impersonate(MK3_SEKTOR, MK3_KANO, "KANO");
					break;
			}
			break;
		case MK3_KABAL:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
			    // Kabal
				case 0: 
					MK3Impersonate(MK3_KABAL, MK3_KABAL, "KABAL");
					break;
				// Cyborg Kabal
				case 1: 
					MK3Impersonate(MK3_SEKTOR, MK3_KABAL, "KABAL");
					break;
			}
			break;
		case MK3_SHAOKHAN:
			if (Subchar>=1) Subchar = 0; // disable reiko because he crashes too much
			switch (Subchar) {
			    // Shao Khan
				case 0: 
					MK3Impersonate(MK3_SHAOKHAN, MK3_SHAOKHAN, "KHAN", "SHAO KHAN");
					break;
				// Reiko
				case 1:
					MK3Impersonate(MK3_SUBZERO, MK3_SHAOKHAN, "REIKO");
					break;
			}
			break;
		case MK3_SEKTOR:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
			    // Cyborg
				case 0: 
					MK3Impersonate(MK3_SEKTOR, MK3_SEKTOR, "SEKTOR");
					break;
				// Human
				case 1:
					MK3Impersonate(MK3_SUBZERO, MK3_SEKTOR, "SEKTOR");
					break;
			}
			break;
		case MK3_CYRAX:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
			    // Cyborg
				case 0: 
					MK3Impersonate(MK3_CYRAX, MK3_CYRAX, "CYRAX");
					break;
				// Human
				case 1:
					MK3Impersonate(MK3_SUBZERO, MK3_CYRAX, "CYRAX");
					break;
			}
			break;
		case MK3_SMOKE:
			if (Subchar>=4) Subchar = 0;
			switch (Subchar) {
			    // Cyborg
				case 0: 
					MK3Impersonate(MK3_SMOKE, MK3_SMOKE, "SMOKE");
					break;
				// Human
				case 1: 
					MK3Impersonate(MK3_SUBZERO, MK3_SMOKE, "SMOKE");
					break;
				// Reptile
				case 2: 
					MK3Impersonate(MK3_SUBZERO, MK3_SMOKE, "REPTILE");
					break;
				// Scorpion
				case 3: 
					MK3Impersonate(MK3_SUBZERO, MK3_SMOKE, "SCORPION");
					break;
			}
			break;
		default:
			Subchar = 0;
			break;
	}
	if (Char==MK3_SONYA && Subchar==1 && !menu) {
		MK3RandomFemale();
	} else if (Char==MK3_SUBZERO && Subchar==4 && !menu) {
		MK3RandomNinja();
	} else MK3SetPal(1, Char, Subchar);
	return Subchar;
}

u32 MK3Input(unsigned short pad) {
	OurHealth = gbReadMemory(0xC0D6);
	OpponentHealth = gbReadMemory(0xC0D7);
	u8 OurChar = gbReadMemory(0xC0F0); // 
	u8 OpponentChar = gbReadMemory(0xC0F1); // also CD40, D526
	OurX = gbReadMemory(0xCD02) | (gbReadMemory(0xCD03) << 8);
	OpponentX = gbReadMemory(0xCD42) | (gbReadMemory(0xCD43) << 8);
	bool InSelectScreen=false, InGame=false;
	if (gbReadMemory(0xC51E)==0x00 && gbReadMemory(0xC522)==0xFF) InSelectScreen = true;
	if (gbReadMemory(0xC080)==0x00 && gbReadMemory(0xC522)==0x00) InGame = true;
	static bool WasInSelectScreen = false;
	static u8 MenuChar = 0;
	static u8 MenuSubChar = 0;
	if (InSelectScreen) MenuChar = gbReadMemory(0xD4CE);
		
	static u8 OldMenuChar = 0;
	// Rumble when they change character
	if (MenuChar != OldMenuChar) {
		if (InSelectScreen && !InGame) {
			systemGameRumble(4);
			MK3SetPal(1,MenuChar);
			MK3RenameEveryoneProperlyExcept(255);
			MenuSubChar=0;
		}
		OldMenuChar = MenuChar;
	}

	// Special Characters in-game
	if (!InSelectScreen) {
		// Set opponent colour
		MK3SetPal(2, OpponentChar);
		// Our colour
		if (MenuSubChar!=0) {
			MK3SetSubchar(MenuChar, MenuSubChar);
		}
	}

	// Get input, and rumble for 2 frames if hurt
	u32 J = GetMKInput(pad, 2);
	if (LK || HK) J |= VBA_BUTTON_A;
	if (LP || HP) J |= VBA_BUTTON_B;
	if (BL) J |= VBA_BUTTON_START;
	if (Throw) {
		if (InGame) J |= VBA_BUTTON_B;
		else J |= VBA_BUTTON_START;
	}
	if (Start) {
		if (InGame) J |= VBA_BUTTON_SELECT;
		else J |= VBA_BUTTON_START;
	}
	if (Select) J |= VBA_BUTTON_SELECT;
	if (InSelectScreen && (Start || Throw || HP || LP || HK || LK || BL)) {
		J |= VBA_BUTTON_START;
	}
	// Fix kick controls to what they should be!
	if (!InSelectScreen && !(J & (VBA_UP | VBA_DOWN))) {
		if (B && HK && !LK) { // Make B+HK do roundhouse, while B+LK does sweep!
			J &= ~VBA_BACK;
			J |= VBA_FORWARD;
		} else if (F && HK) { // Make F+HK do normal high kick
			J &= ~VBA_FORWARD;
		} else if (F && LK) { // Make F+LK also do normal high kick, since no low kicks
			J &= ~VBA_FORWARD;
		}
	}
	// Fix punch controls to what they should be!
	if ((!InSelectScreen) && (J & VBA_DOWN)) {
		// Make D+LP do crouch punch instead of uppercut
		if (LP && !F && !B && !LK && !HK && !HP) { 
			J &= ~VBA_BACK;
			J |= VBA_FORWARD;
		}
	}
	// Run, sometimes does roundhouse kick (Midway's fault, not mine)
	if (CS && InGame) J |= VBA_FORWARD | VBA_BUTTON_A | VBA_BUTTON_B;
	// Allow to choose secret characters from menu
	static bool CancelMovement = false;
	if (InSelectScreen) {
		if ((MenuChar==1 && (J & VBA_DOWN))
		|| (MenuChar==3 && (J & VBA_RIGHT))
		|| (MenuChar==5 && (J & VBA_LEFT))
		|| (MenuChar==7 && (J & VBA_UP))) {
			CancelMovement = true;
			gbWriteMemory(0xD4CE,4);
		} else if (MenuChar==8 && (J & VBA_RIGHT)) {
			gbWriteMemory(0xD4CE,MK3_SHAOKHAN);
		}
		if (CancelMovement) {
			if (J & (VBA_RIGHT | VBA_LEFT | VBA_UP | VBA_DOWN))
				J &= ~(VBA_RIGHT | VBA_LEFT | VBA_UP | VBA_DOWN);
			else CancelMovement = false;
		}
		WasInSelectScreen = true;
	} else {
		CancelMovement = false;
		if (WasInSelectScreen) {
			// We just chose a character, so apply anything special here
			// Cyborg Sub-Zero
			if (MenuChar==MK3_SUBZERO && MenuSubChar==1)
				gbWriteMemory(0xD4CE,MK3_SEKTOR);			
			// Frost
			else if (MenuChar==MK3_SUBZERO && MenuSubChar==3)
				gbWriteMemory(0xD4CE,MK3_SINDEL);			
			// Cyborg Kano
			else if (MenuChar==MK3_KANO && MenuSubChar==1)
				gbWriteMemory(0xD4CE,MK3_SEKTOR);			
			// Cyborg Kabal
			else if (MenuChar==MK3_KABAL && MenuSubChar==1)
				gbWriteMemory(0xD4CE,MK3_SEKTOR);
			// Human Cyrax
			else if (MenuChar==MK3_CYRAX && MenuSubChar==1)
				gbWriteMemory(0xD4CE,MK3_SUBZERO);
			// Human Sektor
			else if (MenuChar==MK3_SEKTOR && MenuSubChar==1)
				gbWriteMemory(0xD4CE,MK3_SUBZERO);
			// Human Smoke, Reptile, Scorpion
			else if (MenuChar==MK3_SMOKE && MenuSubChar>=1)
				gbWriteMemory(0xD4CE,MK3_SUBZERO);
			// Reiko
			else if (MenuChar==MK3_SHAOKHAN && MenuSubChar==1)
				gbWriteMemory(0xD4CE,MK3_SUBZERO);
			WasInSelectScreen = false;
		}
	}

	bool CostumeButton = InSelectScreen && (CS || (J & VBA_BUTTON_SELECT)); // Change Style/Select changes costume
	static bool OldCostumeButton = 0;

	if (CostumeButton && !OldCostumeButton) {
		int OldSubChar = MenuSubChar;
		MenuSubChar = MK3SetSubchar(MenuChar, MenuSubChar+1, true);
		if (MenuSubChar!=OldSubChar) systemGameRumble(8);
	}
	OldCostumeButton = CostumeButton;
	//DebugPrintf("%d,%d C=%d M=%d", MenuChar,MenuSubChar,gbReadMemory(0xC0F0),gbReadMemory(0xCD00));

	return J;
}

u32 MK4Input(unsigned short pad)
{
	OurHealth = gbReadMemory(0xC0D6);
	OpponentHealth = gbReadMemory(0xC0D7);
	u8 OurChar = gbReadMemory(0xC0F0); // also CD00?
	u8 OpponentChar = gbReadMemory(0xC0F1); // also CD40, D526
	OurX = gbReadMemory(0xCD02) | (gbReadMemory(0xCD03) << 8);
	OpponentX = gbReadMemory(0xCD42) | (gbReadMemory(0xCD43) << 8);
	bool InMenu = false; // CAKTODO

	u32 J = GetMKInput(pad);
	if (LK || HK) J |= VBA_BUTTON_A;
	if (LP || HP) J |= VBA_BUTTON_B;
	if (BL) J |= VBA_BUTTON_START;
	if (Start || Select) J |= VBA_BUTTON_SELECT;
	// Fix kick controls to what they should be!
	if (!InMenu && !(J & (VBA_UP | VBA_DOWN))) {
		if (B && HK && !LK) { // Make B+HK do roundhouse, while B+LK does sweep!
			J &= ~VBA_BACK;
			J |= VBA_FORWARD;
		} else if (F && HK) { // Make F+HK do normal high kick
			J &= ~VBA_FORWARD;
		} else if (F && LK) { // Make F+LK also do normal high kick, since no low kicks
			J &= ~VBA_FORWARD;
		}
	}
	// Fix punch controls to what they should be!
	if ((!InMenu) && (J & VBA_DOWN)) {
		// Make D+LP do crouch punch instead of uppercut
		if (LP && !F && !B && !LK && !HK && !HP) { 
			J &= ~VBA_BACK;
			J |= VBA_FORWARD;
		}
	}
	// Run?
	if (CS) J |= VBA_FORWARD | VBA_BUTTON_A | VBA_BUTTON_B;
	if (Throw) J |= VBA_BUTTON_B;
	return J;
}

void MKASetYPos(s16 y) {
	s16 def = 0;
	switch (CPUReadHalfWord(0x200005c)) {
		case 0x5F3C: def = 0x3B; break; // Jax (actually tall not short)
		case 0x89E4: def = 0x39; break; // shang tsung (shortest)
		case 0x704C: def = 0x37; break;
		case 0x3494: case 0x45A4: def = 0x36; break;
		case 0x9AF4: case 0xA37C: def = 0x35; break;
		case 0x67C4: case 0x78D4: case 0x56B4: def = 0x34; break;
		case 0x815C: def = 0x33; break; // sonya
		case 0xB48C: case 0x926C: def = 0x32; break;
		case 0x3D1C: case 0x4E2C: def = 0x31; break;
		case 0xAC04: def = 0x1E; break; // Motaro (tallest)
		default: def = 0x33; break;
	}
	y-=def;
	gbaWriteHalfWord(0x200000A, (u16)((s16)CPUReadHalfWord(0x200000A)+y));
}

bool MKAIsStanding() {
	switch (CPUReadHalfWord(0x2000040)) {
		case 0x039b:
		case 0x03F8:
		case 0x02DF:
		case 0x04BC:
		case 0x04F3:
		case 0x0362: // shao khan
		case 0x01D3: // motaro
		case 0x0427:
		case 0x02A2:
		case 0x052D:
		case 0x0561:
		case 0x034C:
		case 0x0488:
		case 0x0591:
		case 0x0456:
		case 0x0316:
			return true;
		default: 
			return false;
	}
}
void MKARename(u8 n, const char *name) {
#ifndef USE_VM
	if (n>=MKA_SubZero2) n--; // second sub zero is not in names list!
	u32 addr = 0x80285CC+n*16;
	char *s = (char *)&rom[addr & 0x1FFFFFF];
	int L = strlen(s)-1-strlen(name);
	int i;
	strcpy(s, "            ");
	strncpy(s, name, 12);
	strcat(s, " ");
	for (i=0; i<L; i++) strcat(s, " ");
	for (int i=strlen(s)+1; i<12; i++) s[i]=' ';
	s[12]='\0';
#endif
}
void MKARenameEveryoneProperlyExcept(u8 n) {
	const char *names[MKA_ShaoKhan+1] = {
		"RAIN", "REPTILE", "STRYKER", "JAX", "NIGHT WOLF", "JADE", "NOOB SAIBOT", "SONYA",
		"KANO", "MILEENA", "ERMAC", "SUB ZERO", "SUB ZERO", "KUNG LAO", "SEKTOR", "KITANA", "SMOKE",
		"SCORPION", "CYRAX", "KABAL", "SINDEL", "SMOKE", "LIU KANG", "SHANG TSUNG", "MOTARO",
		"SHAO KHAN"};
	int i;
	if (n==MKA_SubZero || n==MKA_SubZero2) {
		for (i=0; i<=MKA_ShaoKhan; i++) {
			if (i!=MKA_SubZero && i!=MKA_SubZero2) MKARename(i, names[i]);
		}
	} else {
		for (i=0; i<=MKA_ShaoKhan; i++) {
			if (i!=n) MKARename(i, names[i]);
		}
	}
}

void MKAMakeRainCyborg() {
    // cyrax victory pose
	gbaWriteHalfWord(0x87EBE64, 0x03B8); 
	gbaWriteHalfWord(0x87EBE6C, 0x0506);
	gbaWriteMemory(0x87EBE78, 0x87E33BD);
	// cyborg open chest pose
	gbaWriteHalfWord(0x87EBE7C, 0x03C1); 
	gbaWriteHalfWord(0x87EBE84, 0x0502);
	gbaWriteHalfWord(0x87EBE88, 0x01);
	gbaWriteMemory(0x87EBE90, 0x87E33C1);	
	// partial cyborg victory pose (instead of raising one hand to sky)
	gbaWriteHalfWord(0x87EBE94, 0x03B8); 
	gbaWriteHalfWord(0x87EBE9C, 0x0505); 
	gbaWriteHalfWord(0x87EBEA0, 0x03); 
	gbaWriteMemory(0x87EBEA8, 0x87E338E);
}
void MKAMakeRainHuman() {
    // reptile victory pose
	gbaWriteHalfWord(0x87EBE64, 0x0374); 
	gbaWriteHalfWord(0x87EBE6C, 0x0505);
	gbaWriteMemory(0x87EBE78, 0x87E33BE);
	// ninja shoot magic pose
	gbaWriteHalfWord(0x87EBE7C, 0x03B3); 
	gbaWriteHalfWord(0x87EBE84, 0x0503);
	gbaWriteHalfWord(0x87EBE88, 0x02);
	gbaWriteMemory(0x87EBE90, 0x87E33CE);	
	// ninja raising one hand to sky
	gbaWriteHalfWord(0x87EBE94, 0x036E); 
	gbaWriteHalfWord(0x87EBE9C, 0x0505); 
	gbaWriteHalfWord(0x87EBEA0, 0x03); 
	gbaWriteMemory(0x87EBEA8, 0x87E338E);
}
void MKAMakeRainUnmasked() {
    // unmasked victory pose
	gbaWriteHalfWord(0x87EBE64, 0x02E8); 
	gbaWriteHalfWord(0x87EBE6C, 0x0504);
	gbaWriteMemory(0x87EBE78, 0x87E33BF);
	// unmasked shoot magic pose
	gbaWriteHalfWord(0x87EBE7C, 0x02E5); 
	gbaWriteHalfWord(0x87EBE84, 0x0503);
	gbaWriteHalfWord(0x87EBE88, 0x02);
	gbaWriteMemory(0x87EBE90, 0x87E33CE);
	// unmasked raising both hands to sky
	gbaWriteHalfWord(0x87EBE94, 0x02DC); 
	gbaWriteHalfWord(0x87EBE9C, 0x0503); 
	gbaWriteHalfWord(0x87EBEA0, 0x02); 
	gbaWriteMemory(0x87EBEA8, 0x87E3385);
}


void MKAMakeNinja() {
	if (MKAIsStanding()) {
	    MKASetYPos(0x36); // y offset
	    gbaWriteHalfWord(0x2000040, 0x039b);
	    gbaWriteHalfWord(0x2000048, 0x0409);
	}
	gbaWriteHalfWord(0x200005c, 0x3494);
}
void MKAMakeCyborg() {
	if (MKAIsStanding()) {
		MKASetYPos(0x31); // y offset
		gbaWriteHalfWord(0x2000040, 0x03F8);
		gbaWriteHalfWord(0x2000048, 0x0509);
	}
	gbaWriteHalfWord(0x200005c, 0x3D1C);
}
void MKAMakeUnmasked() {
	if (MKAIsStanding()) {
		MKASetYPos(0x35); // y offset
		gbaWriteHalfWord(0x2000040, 0x02DF);
		gbaWriteHalfWord(0x2000048, 0x0509);
	}
	gbaWriteHalfWord(0x200005c, 0x9AF4);
}
void MKAMakeFemale() {
	if (MKAIsStanding()) {
		MKASetYPos(0x35); // y offset
		gbaWriteHalfWord(0x2000040, 0x04BC);
		gbaWriteHalfWord(0x2000048, 0x0608);
	}
	gbaWriteHalfWord(0x200005c, 0xA37C);	
}
void MKAMakeKano() {
	if (MKAIsStanding()) {
		MKASetYPos(0x34); // y offset
		gbaWriteHalfWord(0x2000040, 0x04F3);
		gbaWriteHalfWord(0x2000048, 0x0509);
	}
	gbaWriteHalfWord(0x200005c, 0x67C4);
}
void MKAMakeShaoKhan() {
	if (MKAIsStanding()) {
		MKASetYPos(0x32); // y offset
		gbaWriteHalfWord(0x2000040, 0x0362);
		gbaWriteHalfWord(0x2000048, 0x0407);
	}
	gbaWriteHalfWord(0x200005c, 0xB48C);
}
void MKAMakeMotaro() {
	if (MKAIsStanding()) {
		MKASetYPos(0x1E); // y offset
		gbaWriteHalfWord(0x2000040, 0x01D3);
		gbaWriteHalfWord(0x2000048, 0x040B);
	}
	gbaWriteHalfWord(0x200005c, 0xAC04);
}
void MKAMakeLiuKang() {
	if (MKAIsStanding()) {
		MKASetYPos(0x36); // y offset
		gbaWriteHalfWord(0x2000040, 0x0427);
		gbaWriteHalfWord(0x2000048, 0x0411);
	}
	gbaWriteHalfWord(0x200005c, 0x45A4);
}
void MKAMakeShangTsung() {
	if (MKAIsStanding()) {
		MKASetYPos(0x39); // y offset
		gbaWriteHalfWord(0x2000040, 0x0316);
		gbaWriteHalfWord(0x2000048, 0x0905);
	}
	gbaWriteHalfWord(0x200005c, 0x89E4);
}
void MKAMakeJax() {
	if (MKAIsStanding()) {
		MKASetYPos(0x3B); // y offset
		gbaWriteHalfWord(0x2000040, 0x052D);
		gbaWriteHalfWord(0x2000048, 0x0508);
	}
	gbaWriteHalfWord(0x200005c, 0x5F3C);
}
void MKAMakeSindel() {
	if (MKAIsStanding()) {
		MKASetYPos(0x34); // y offset
		gbaWriteHalfWord(0x2000040, 0x0456);
		gbaWriteHalfWord(0x2000048, 0x0509);
	}
	gbaWriteHalfWord(0x200005c, 0x78D4);
}
void MKAMakeKabal() { 
	if (MKAIsStanding()) {
	    MKASetYPos(0x31); // y offset
	    gbaWriteHalfWord(0x2000040, 0x0591);
	    gbaWriteHalfWord(0x2000048, 0x050A);
	}
	gbaWriteHalfWord(0x200005c, 0x4E2C);
}

#define CYBORG_RED      0x94
#define CYBORG_DARKRED  0x95
#define CYBORG_GREY     0x96
#define CYBORG_GREY2    0x97
#define CYBORG_YELLOW   0x9A
#define CYBORG_YELLOW2  0x9B

#define NINJA_BROWNRED 0x04
#define NINJA_REDGREY 0x06
#define NINJA_ICE 0x08
#define NINJA_WHITE 0x0A
#define NINJA_GREY 0x0B
#define NINJA_PINKPURPLE 0x8E
#define NINJA_PURPLE 0x8F
#define NINJA_GREEN 0x90
#define NINJA_GREENISH 0x91
#define NINJA_RED 0x9C
#define NINJA_BRIGHTRED 0x9D
#define SHADOW_BLACK 0x9E
#define SHADOW_GREY 0x9F
#define NINJA_BLUE 0xA0
#define NINJA_BLUE2 0xA1
#define NINJA_YELLOW 0xA2
#define NINJA_YELLOW2 0xA3
#define NINJA_BROWN 0x98
#define NINJA_BROWN2 0x99

#define UNMASKED_PURPLE 0xA8
#define UNMASKED_DARKGREEN 0xA9
#define UNMASKED_BLUE 0xB6
#define UNMASKED_BRIGHTGREEN 0xB7

#define FEMALE_FADEDBLUE 0x1A
#define FEMALE_BLUE 0x1B
#define FEMALE_CYAN 0x1C
#define FEMALE_GREEN 0x1D
#define FEMALE_YELLOW 0x1F
#define FEMALE_PURPLE 0x20
#define FEMALE_PINK 0x21

#define BOSS_RED 0x1E
#define BOSS_YELLOW 0xB2
#define BOSS_PINK 0xB3


void MKAChangeColour(u8 colour) {
	gbaWriteHalfWord(0x2000004, colour);
}

void MKARandomNinja() {
	// 7: SubZero, Scorpion, Reptile, Smoke, Ermac, Rain, Noob Saibot, 
	// +3: Tremor, Cyrax, Sektor
	switch (rand() % 10) {
		case 0: MKAChangeColour(NINJA_BLUE); break;
		case 1: MKAChangeColour(NINJA_YELLOW); break;
		case 2: MKAChangeColour(NINJA_GREEN); break;
		case 3: MKAChangeColour(NINJA_WHITE); break;
		case 4: MKAChangeColour(NINJA_RED); break;
		case 5: MKAChangeColour(NINJA_PURPLE); break;
		case 6: MKAChangeColour(SHADOW_GREY); break;

		case 7: MKAChangeColour(NINJA_BROWN); break;
		case 8: MKAChangeColour(CYBORG_YELLOW); break;
		case 9: MKAChangeColour(CYBORG_RED); break;		
	}
}
void MKARandomCyborg() {
	// 3: Cyrax, Sektor, Smoke
	// +6: SubZero, Scorpion, Reptile, Ermac, Rain, Noob Saibot, 
	// +1: Tremor (Note, Jax and Kano can also be cyborgs but are not ninjas like Chameleon)
	switch (rand() % 10) {
		case 0: MKAChangeColour(CYBORG_YELLOW); break;
		case 1: MKAChangeColour(CYBORG_RED); break;
		case 2: MKAChangeColour(CYBORG_GREY); break;		

		case 3: MKAChangeColour(NINJA_BLUE); break;
		case 4: MKAChangeColour(CYBORG_YELLOW2); break;
		case 5: MKAChangeColour(NINJA_GREEN); break;
		case 6: MKAChangeColour(CYBORG_DARKRED); break;
		case 7: MKAChangeColour(NINJA_PINKPURPLE); break;
		case 8: MKAChangeColour(SHADOW_GREY); break;
		case 9: MKAChangeColour(NINJA_BROWN); break;
	}
}
void MKARandomFemale() {
	// 3: Kitana, Mileena, Jade
	// +3: Tanya, Ruby, Skarlet
	// (Note, Sonya, Sindel, and Frost are also females, but not ninjas like Khameleon)
	switch (rand() % 6) {
		case 0: MKAChangeColour(FEMALE_BLUE); break;
		case 1: MKAChangeColour(FEMALE_PURPLE); break;
		case 2: MKAChangeColour(FEMALE_GREEN); break;		

		case 3: MKAChangeColour(NINJA_YELLOW); break;
		case 4: MKAChangeColour(NINJA_RED); break;
		case 5: MKAChangeColour(NINJA_BRIGHTRED); break;
	}
}

u8 MKANextSubchar(int Char, int Subchar, u16 OriginalColour) {
	Subchar++;
	switch (Char) {
		case MKA_Reptile:
			if (Subchar>=5) Subchar = 0;
			switch (Subchar) {
			    // Reptile
				case 0: 
					MKARename(MKA_Reptile, "REPTILE");
					MKAMakeNinja();
					MKAChangeColour(OriginalColour);
					break;
				case 1: 
					MKARename(MKA_Reptile, "REPTILE");
					MKAMakeCyborg();
					MKAChangeColour(NINJA_GREEN);
					break;
				case 2: 
					MKARename(MKA_Reptile, "REPTILE");
					MKAMakeUnmasked();
					MKAChangeColour(UNMASKED_DARKGREEN); // different from green subzero
					break;
				// Chameleon
				case 3: 
					MKARename(MKA_Reptile, "CHAMELEON");
					MKAMakeNinja();
					MKAChangeColour(NINJA_GREY);
					break;
				case 4: 
					MKARename(MKA_Reptile, "CHAMELEON");
					MKAMakeCyborg();
					MKAChangeColour(CYBORG_GREY);
					break;
			}
			break;
		case MKA_NoobSaibot:
			if (Subchar>=4) Subchar = 0;
			switch (Subchar) {
			    // Noob-Saibot
				case 0: 
					MKAMakeNinja();
					MKAChangeColour(OriginalColour);
					gbaWriteByte(0x202F6A8, MKA_NoobSaibot);
					gbaWriteByte(0x2000025, MKA_NoobSaibot);
					MKARename(MKA_Kano, "KANO");
					break;
				// As a Cyborg
				case 1: 
					MKAMakeCyborg();
					MKAChangeColour(SHADOW_GREY);
					gbaWriteByte(0x202F6A8, MKA_NoobSaibot);
					gbaWriteByte(0x2000025, MKA_NoobSaibot);
					MKARename(MKA_Kano, "KANO");
					break;
				// Looks like Kano
				case 2: 
					MKAMakeKano();
					MKAChangeColour(SHADOW_BLACK);
					gbaWriteByte(0x202F6A8, MKA_Kano);
					gbaWriteByte(0x2000025, MKA_Kano);
					MKARename(MKA_Kano, "NOOB SAIBOT");
					break;
				// Looks like Classic Sub Zero (because he is)
				case 3: 
					MKAMakeNinja();
					MKAChangeColour(NINJA_BLUE);
					gbaWriteByte(0x202F6A8, MKA_NoobSaibot);
					gbaWriteByte(0x2000025, MKA_NoobSaibot);
					MKARename(MKA_Kano, "KANO");
					break;
			}
			break;
		case MKA_Ermac:
			if (Subchar>=3) Subchar = 0;
			switch (Subchar) {
			    // Ermac
				case 0: 
					MKARename(MKA_Ermac, "ERMAC");
					MKAMakeNinja();
					MKAChangeColour(OriginalColour);
					break;
				case 1: 
					MKARename(MKA_Ermac, "ERMAC");
					MKAMakeCyborg();
					MKAChangeColour(CYBORG_RED);
					break;
				case 2: 
					MKARename(MKA_Ermac, "RUBY");
					MKAMakeFemale();
					MKAChangeColour(NINJA_RED);
					break;
			}
			break;
		case MKA_SubZero:
			if (Subchar>=5) Subchar = 0;
			switch (Subchar) {
			    // Classic Subzero
				case 0: 
					MKARename(MKA_SubZero, "SUB ZERO");
					MKAMakeNinja();
					MKAChangeColour(OriginalColour);
					break;
				case 1: 
					MKARename(MKA_SubZero, "SUB ZERO");
					MKAMakeCyborg();
					MKAChangeColour(NINJA_BLUE);
					break;
				case 2: 
					MKARename(MKA_SubZero, "SUB ZERO");
					MKAMakeUnmasked();
					MKAChangeColour(UNMASKED_BLUE);
					break;
				// Looking like Noob Saibot (which he is)
				case 3: 
					MKARename(MKA_SubZero, "SUB ZERO");
					MKAMakeNinja();
					MKAChangeColour(SHADOW_BLACK);
					break;
				// Frost
				case 4: 
					MKARename(MKA_SubZero, "FROST");
					MKAMakeFemale();
					MKAChangeColour(NINJA_ICE);
					break;
			}
			break;
		case MKA_SubZero2:
			if (Subchar>=4) Subchar = 0; 
			switch (Subchar) {
			    // Subzero Unmasked
				case 0: 
					MKARename(MKA_SubZero2, "SUB ZERO");
					MKAMakeUnmasked();
					MKAChangeColour(OriginalColour);
					break;
				// As a cyborg
				case 1: 
					MKARename(MKA_SubZero2, "SUB ZERO");
					MKAMakeCyborg();
					MKAChangeColour(NINJA_BLUE);
					break;
				// Masked in his MK2 coloured costume
				case 2: 
					MKARename(MKA_SubZero2, "SUB ZERO");
					MKAMakeNinja();
					MKAChangeColour(FEMALE_CYAN);
					break;
				// Frost
				case 3: 
					MKARename(MKA_SubZero2, "FROST");
					MKAMakeFemale();
					MKAChangeColour(NINJA_ICE);
					break;
			}
			break;
		case MKA_Scorpion:
			if (Subchar>=3) Subchar = 0;
			switch (Subchar) {
			    // Human Scorpion
				case 0: 
					MKARename(MKA_Scorpion, "SCORPION");
					MKAMakeNinja();
					MKAChangeColour(OriginalColour);
					break;
				// As a cyborg
				case 1: 
					MKARename(MKA_Scorpion, "SCORPION");
					MKAMakeCyborg();
					MKAChangeColour(CYBORG_YELLOW2);
					break;
				// Monster
				case 2: 
					MKARename(MKA_Scorpion, "MONSTER");
					MKAMakeShaoKhan();
					MKAChangeColour(SHADOW_GREY);
					break;
			}
			break;
		case MKA_Mystery:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
				// Human Smoke
				case 0: 
					MKARename(MKA_Mystery, "SMOKE");
					gbaWriteByte(0x20001A8, 1); // unlock
					MKAMakeNinja();
					MKAChangeColour(OriginalColour);
					gbaWriteByte(0x202F6A8, MKA_Mystery);
					gbaWriteByte(0x2000025, MKA_Mystery);
					break;
				// Looking like a Cyborg
				case 1: 
					MKARename(MKA_Mystery, "SMOKE");
					MKAMakeCyborg();
					MKAChangeColour(CYBORG_GREY);
					gbaWriteByte(0x202F6A8, MKA_Smoke);
					gbaWriteByte(0x2000025, MKA_Smoke);
					break;
			}
			break;
		case MKA_Rain:
			if (Subchar>=3) Subchar = 0;
			switch (Subchar) {
				// Human Rain
				case 0:
					MKARename(MKA_Rain, "RAIN");
					MKARename(MKA_Jax, "JAX");
					MKAMakeRainHuman();
					MKAMakeNinja();
					MKAChangeColour(OriginalColour);
					gbaWriteByte(0x202F6A8, MKA_Rain);
					gbaWriteByte(0x2000025, MKA_Rain);
					break;
				// Cyborg Rain
				case 1:
					MKARename(MKA_Rain, "RAIN");
					MKARename(MKA_Jax, "JAX");
					MKAMakeRainCyborg();
					MKAMakeCyborg();
					MKAChangeColour(NINJA_PINKPURPLE);
					gbaWriteByte(0x202F6A8, MKA_Rain);
					gbaWriteByte(0x2000025, MKA_Rain);
					break;
				// Unmasked Rain
				case 2:
					MKARename(MKA_Rain, "RAIN");
					MKARename(MKA_Jax,  "JAX");
					MKAMakeRainUnmasked();
					MKAMakeUnmasked();
					MKAChangeColour(UNMASKED_PURPLE);
					gbaWriteByte(0x202F6A8, MKA_Rain);
					gbaWriteByte(0x2000025, MKA_Rain);
					break;
				// Tremor
				case 3:
					MKARename(MKA_Rain, "TREMOR");
					MKARename(MKA_Jax, "TREMOR");
					MKAMakeNinja();
					MKAChangeColour(NINJA_BROWN);
					gbaWriteByte(0x202F6A8, MKA_Jax);
					gbaWriteByte(0x2000025, MKA_Jax);
					break;
				case 4: 
					MKARename(MKA_Rain, "TREMOR");
					MKARename(MKA_Jax, "TREMOR");
					MKAMakeCyborg();
					MKAChangeColour(NINJA_BROWN);
					gbaWriteByte(0x202F6A8, MKA_Jax);
					gbaWriteByte(0x2000025, MKA_Jax);
					break;
			}
			break;
		case MKA_Sektor:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
				// Cyborg Sektor
				case 0: 
					MKAMakeCyborg();
					MKAChangeColour(OriginalColour);
					break;
				// Human Sektor
				case 1: 
					MKAMakeNinja();
					MKAChangeColour(NINJA_RED);
					break;
			}
			break;
		case MKA_Cyrax:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
				// Cyborg Cyrax
				case 0: 
					MKAMakeCyborg();
					MKAChangeColour(OriginalColour);
					break;
				// Human Cyrax
				case 1: 
					MKAMakeNinja();
					MKAChangeColour(OriginalColour);
					break;
			}
			break;
		case MKA_Kano:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
				// Human Kano
				case 0: 
					MKAMakeKano();
					MKAChangeColour(OriginalColour);
					break;
				// Cyborg Kano
				case 1: 
					MKAMakeCyborg();
					MKAChangeColour(CYBORG_DARKRED);
					break;
			}
			break;
		case MKA_Kabal:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
				// Human Kabal
				case 0: 
					MKAMakeKabal();
					MKAChangeColour(OriginalColour);
					break;
				// Cyborg Kabal
				case 1: 
					MKAMakeCyborg();
					MKAChangeColour(OriginalColour); // Brown cyborg, like Tremor
					break;
			}
			break;
		case MKA_Jax:
			if (Subchar>=3) Subchar = 0;
			switch (Subchar) {
				// Half Cyborg Jax
				case 0: 
					MKAMakeJax();
					MKAChangeColour(OriginalColour);
					break;
				// Human Jax (a bit glitchy, but OK)
				case 1: 
					MKAMakeJax();
					MKAChangeColour(NINJA_YELLOW2);
					break;
				// Cyborg Jax
				case 2: 
					MKAMakeCyborg();
					MKAChangeColour(NINJA_WHITE);
					break;
			}
			break;
		case MKA_Smoke:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
				// Cyborg Smoke
				case 0: 
					MKAMakeCyborg();
					MKAChangeColour(OriginalColour);
					gbaWriteByte(0x202F6A8, MKA_Smoke);
					gbaWriteByte(0x2000025, MKA_Smoke);
					break;
				// Human Smoke
				case 1: 
					gbaWriteByte(0x20001A8, 1);
					MKAMakeNinja();
					MKAChangeColour(NINJA_WHITE);
					gbaWriteByte(0x202F6A8, MKA_Mystery);
					gbaWriteByte(0x2000025, MKA_Mystery);
					break;
			}
			break;
		case MKA_Jade:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
				// Jade
				case 0: 
					MKARename(MKA_Jade, "JADE");
					//MKAMakeFemale();
					MKAChangeColour(OriginalColour);
					break;
				// Khameleon
				case 1: 
					MKARename(MKA_Jade, "KHAMELEON");
					//MKAMakeFemale();
					MKAChangeColour(NINJA_GREY);
					break;
			}
			break;
		case MKA_Kitana:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
				// Kitana
				case 0: 
					MKARename(MKA_Kitana, "KITANA");
					//MKAMakeFemale();
					MKAChangeColour(OriginalColour);
					break;
				// Skarlet (a red version of kitana, supposed to be faster but isn't here)
				case 1: 
					MKARename(MKA_Kitana, "SKARLET");
					//MKAMakeFemale();
					MKAChangeColour(NINJA_BRIGHTRED);
					break;
			}
			break;
		case MKA_Mileena:
			if (Subchar>=2) Subchar = 0;
			switch (Subchar) {
				// Mileena
				case 0: 
					MKARename(MKA_Mileena, "MILEENA");
					MKARename(MKA_LiuKang, "LIU KANG");
					MKAMakeFemale();
					MKAChangeColour(OriginalColour);
					gbaWriteByte(0x202F6A8, MKA_Mileena);
					gbaWriteByte(0x2000025, MKA_Mileena);
					break;
				// Tanya
				case 1: 
					MKARename(MKA_Mileena, "TANYA");
					MKARename(MKA_LiuKang, "TANYA");
					MKAMakeFemale();
					MKAChangeColour(NINJA_YELLOW);
					gbaWriteByte(0x202F6A8, MKA_LiuKang);
					gbaWriteByte(0x2000025, MKA_LiuKang);
					break;
			}
			break;
		case MKA_ShangTsung:
			if (Subchar>=3) Subchar = 0;
			switch (Subchar) {
				// Shang Tsung
				case 0: 
					MKARename(MKA_ShangTsung, "SHANG TSUNG");
					MKAMakeShangTsung();
					MKAChangeColour(OriginalColour);
					gbaWriteByte(0x202F6A8, MKA_ShangTsung);
					gbaWriteByte(0x2000025, MKA_ShangTsung);
					break;
				// Motaro
				case 1:
					gbaWriteByte(0x20001A8, 2); // unlock
					MKARename(MKA_ShangTsung, "MOTARO");
					MKAMakeMotaro();
					MKAChangeColour(NINJA_BROWN);
					gbaWriteByte(0x202F6A8, MKA_Motaro);
					gbaWriteByte(0x2000025, MKA_Motaro);
					//MKAMakeShaoKhan();
					//gbaWriteByte(0x202F6A8, MKA_ShaoKhan);
					break;
				// Shao Khan
				case 2:
					gbaWriteByte(0x20001A8, 3); // unlock
					MKARename(MKA_ShangTsung, "SHAO KHAN");
					MKARename(MKA_ShaoKhan, "SHAO KHAN");
					MKAChangeColour(BOSS_RED);
					gbaWriteByte(0x202F6A8, MKA_ShaoKhan);
					gbaWriteByte(0x2000025, MKA_ShaoKhan);
					MKAMakeShaoKhan();
					//gbaWriteByte(0x202F6A8, MKA_ShaoKhan);
					break;
				// Reiko
				case 3:
					gbaWriteByte(0x20001A8, 3); // unlock Shao Khan
					MKARename(MKA_ShangTsung, "REIKO");
					MKARename(MKA_ShaoKhan, "REIKO");
					gbaWriteByte(0x202F6A8, MKA_ShaoKhan);
					gbaWriteByte(0x2000025, MKA_ShaoKhan);
					MKAMakeNinja();
					MKAChangeColour(FEMALE_PURPLE);
					//gbaWriteByte(0x202F6A8, MKA_ShaoKhan);
					break;
			}
			break;
		case MKA_LiuKang:
			if (Subchar>=4) Subchar = 0;
			switch (Subchar) {
			    // Liu Kang
				case 0: 
					MKARename(MKA_LiuKang, "LIU KANG");
					MKAMakeLiuKang();
					MKAChangeColour(OriginalColour);
					gbaWriteByte(0x202F6A8, MKA_LiuKang);
					gbaWriteByte(0x2000025, MKA_LiuKang);
					break;
				// Johnny Cage
				case 1: 
					MKARename(MKA_LiuKang, "JOHNNY CAGE");
					MKAMakeLiuKang();
					MKAChangeColour(0x92); // Stryker's Blue
					gbaWriteByte(0x202F6A8, MKA_Jade);
					gbaWriteByte(0x2000025, MKA_Jade);
					break;
				// Blaze
				case 2: 
					MKARename(MKA_LiuKang, "BLAZE");
					MKAMakeLiuKang();
					MKAChangeColour(0x63); // Yellowy orange
					gbaWriteByte(0x202F6A8, MKA_LiuKang);
					gbaWriteByte(0x2000025, MKA_LiuKang);
					break;
				// Hornbuckle
				case 3: 
					MKARename(MKA_LiuKang, "HORNBUCKLE");
					MKAMakeLiuKang();
					MKAChangeColour(UNMASKED_DARKGREEN); 
					gbaWriteByte(0x202F6A8, MKA_LiuKang);
					gbaWriteByte(0x2000025, MKA_LiuKang);
					break;
			}
			break;
		default:
			Subchar = 0;
			break;
	}
	return Subchar;
}

u32 MKAInput(unsigned short pad)
{
	bool InMenu= false;
	if (CPUReadHalfWord(0x2000008)==0xFFFC) InMenu=true;
	else InMenu = false;
	
	OurHealth = CPUReadByte(0x2000020);
	OpponentHealth = CPUReadByte(0x2000020+0x68);
	OurX = (s16)CPUReadHalfWord(0x2000008);
	OpponentX = (s16)CPUReadHalfWord(0x2000008+0x68);
	u8 OurChar =   CPUReadByte(0x2000025); // also 202f6a8
	u8 OpponentChar = CPUReadByte(0x2000025+0x68);
		
	// Special characters
	static int MenuChar = 0;
	static int MenuSubchar = 0;
	static bool WasInMenu = false;
	static u8 OurOldChar = 255;
	static u8 OriginalColour = 0;
	static bool OldCostumeButton = false;
	bool CostumeButton = false;
	if (OriginalColour == 0) OriginalColour = CPUReadByte(0x2000004);
	// Manually changed characters, so reset MenuChar and SubChar
	if (InMenu && OurChar!=OurOldChar && OurChar!=0) {
		//DebugPrintf("%d %d %d", OurChar, OurOldChar, MenuChar);
		MenuChar = OurChar;
		MenuSubchar = 0;
		OriginalColour = CPUReadByte(0x2000004);
		systemGameRumble(4);
		MKARenameEveryoneProperlyExcept(255);
	} else if (InMenu && OurChar!=OurOldChar) {
		// Either changing character to Rain, or more likely starting combat
		// if changing to rain, they might actually end up changing to Cyborg Rain or Tremor
		MenuChar = OurChar;
		OriginalColour = CPUReadByte(0x2000004);
	}
	
	// Special Characters in-game
	if (!InMenu) {
	    // in the game we just changed from 0 back to our real character
		// unless OurChar is Jax and Subchar is 3 or 4 in which case MenuChar should be 0
		// So check for Tremor, also check for Shao Khan
		if (OurOldChar==0 && OurChar!=0) {
			if (OurChar==MKA_Jax && (MenuSubchar==3 || MenuSubchar==4))
				MenuChar=0;
			else if (OurChar==MKA_ShaoKhan && (MenuSubchar==2 || MenuSubchar==3))
				MenuChar=MKA_ShangTsung;
			else
				MenuChar=OurChar;
		}
		if (MenuSubchar!=0) {
			//u8 OldMaxFrame = CPUReadByte(0x2000048);
			MKANextSubchar(MenuChar, MenuSubchar-1, OriginalColour);
			if (OurOldChar==0 && OurChar!=0) MKARenameEveryoneProperlyExcept(OurChar);
			if (CPUReadByte(0x200001D)>=CPUReadByte(0x2000048))
				gbaWriteByte(0x200001D,CPUReadByte(0x2000048)-1);			
			//if (CPUReadByte(0x200001D)>=OldMaxFrame && OldMaxFrame>0)
			//	gbaWriteByte(0x200001D,OldMaxFrame-1);			
			if (MenuChar==MKA_Reptile && MenuSubchar==3)
				MKARandomNinja();
			else if (MenuChar==MKA_Reptile && MenuSubchar==4)
				MKARandomCyborg();
			else if (MenuChar==MKA_Jade && MenuSubchar==1)
				MKARandomFemale();
		} else {
			if (OurOldChar==0 && OurChar!=0)
				MKARenameEveryoneProperlyExcept(OurChar);
		}
		OurOldChar=CPUReadByte(0x2000025);
	}
	WasInMenu = InMenu;
	
	//DebugPrintf("M=%d O=%d MC=%d,%d Old=%d",InMenu,OurChar,MenuChar,MenuSubchar,OurOldChar);
	
	//  CONTROLS
	u32 J = GetMKInput(pad);
	if (LK || HK) J |= VBA_BUTTON_A;
	if (LP || HP) J |= VBA_BUTTON_B;
	if (BL) J |= VBA_BUTTON_R;
	if (CS) J |= VBA_BUTTON_L;
	if (Throw) {
		if (InMenu) J |= VBA_BUTTON_A;
		else J |= VBA_FORWARD | VBA_BUTTON_B;
	} 
	if (Start) J |= VBA_BUTTON_START;
	if (Select) J |= VBA_BUTTON_SELECT;
	// Fix kick controls to what they should be!
	if (!InMenu && !(J & (VBA_UP | VBA_DOWN))) {
		if (B && LK && !HK) { // Make B+HK do roundhouse, while B+LK does sweep!
			J |= VBA_DOWN;
		}
	} else if (!InMenu && (J & VBA_DOWN)) { // Make D+HK do crouch HK, while D+LK does crouch LK
		if (HK && !LK && !B && !F && !LP && !HP) { 
			J &= ~VBA_BACK;
			J |= VBA_FORWARD;
		}
	}
	// Fix punch controls to what they should be!
	if ((!InMenu) && (J & VBA_DOWN)) {
		// Make D+LP do crouch punch instead of uppercut
		if (LP && !F && !B && !LK && !HK && !HP) { 
			J &= ~VBA_BACK;
			J |= VBA_FORWARD;
		}
	}

	CostumeButton = InMenu && (CS || (J & VBA_BUTTON_SELECT)); // Change Style/Select changes costume

	if (CostumeButton && !OldCostumeButton) {
		int OldSubChar = MenuSubchar;
		u8 OldMaxFrame = CPUReadByte(0x2000048);
		MenuSubchar = MKANextSubchar(MenuChar, MenuSubchar, OriginalColour);
		if (MenuSubchar!=OldSubChar) systemGameRumble(8);
		OurOldChar = CPUReadByte(0x2000025);
		// apply change instantly and safely by skipping to last frame of new animation
		gbaWriteByte(0x200001D,CPUReadByte(0x2000048)-1);
		if (CPUReadByte(0x200001D)>=OldMaxFrame && OldMaxFrame>0)
			gbaWriteByte(0x200001D,OldMaxFrame-1);			
	}
	OurOldChar = CPUReadByte(0x2000025);
	OldCostumeButton = CostumeButton;
	
	return J;
}

u32 MKDAInput(unsigned short pad)
{
	static u32 prevJ = 0, prevPrevJ = 0;
	OurHealth = CPUReadByte(0x3000760); // 731 or 760
	u8 Side = CPUReadByte(0x3000747);
	u32 Forwards, Back;
	if (Side == 0) {
		OurX = 0; OpponentX = 1;
		Forwards = VBA_RIGHT;
		Back = VBA_LEFT;
	} else {
		OurX = 1; OpponentX = 0;
		Forwards = VBA_LEFT;
		Back = VBA_RIGHT;
	}

	u32 J = GetMKInput(pad, 10);
	if (HP || LP) J |= VBA_BUTTON_B;
	if (HK || LK) J |= VBA_BUTTON_A;
	if (BL) J |= VBA_BUTTON_R;
	if (CS) J |= VBA_BUTTON_L;
	if (Select) J |= VBA_BUTTON_SELECT;
	if (Start) J |= VBA_BUTTON_START;
	if (Throw) {
		if ((prevJ & Forwards && prevJ & VBA_BUTTON_A && prevJ & VBA_BUTTON_B) || ((prevPrevJ & Forwards) && !(prevJ & Forwards)))
			J |= Forwards | VBA_BUTTON_A | VBA_BUTTON_B; // R, R+1+2 = throw

		else if (prevJ & Forwards) {
			J &= ~Forwards;
			J &= ~VBA_BUTTON_A;
			J &= ~VBA_BUTTON_B;
		} else
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

u32 MKTEInput(unsigned short pad)
{
	static u32 prevJ = 0, prevPrevJ = 0;
	OurHealth = CPUReadByte(0x3000760); // 731 or 760
	u8 Side = CPUReadByte(0x3000777);
	u32 Forwards, Back;
	if (Side == 0) {
		OurX = 0; OpponentX = 1;
		Forwards = VBA_RIGHT;
		Back = VBA_LEFT;
	} else {
		OurX = 1; OpponentX = 0;
		Forwards = VBA_LEFT;
		Back = VBA_RIGHT;
	}

	u32 J = GetMKInput(pad, 10);
	if (HP || LP) J |= VBA_BUTTON_B;
	if (HK || LK) J |= VBA_BUTTON_A;
	if (BL) J |= VBA_BUTTON_R;
	if (CS) J |= VBA_BUTTON_L;
	if (Select) J |= VBA_BUTTON_SELECT;
	if (Start) J |= VBA_BUTTON_START;
	if (Throw) {
		if ((prevJ & Forwards && prevJ & VBA_BUTTON_A && prevJ & VBA_BUTTON_B) || ((prevPrevJ & Forwards) && !(prevJ & Forwards)))
			J |= Forwards | VBA_BUTTON_A | VBA_BUTTON_B; // R, R+1+2 = throw

		else if (prevJ & Forwards) {
			J &= ~Forwards;
			J &= ~VBA_BUTTON_A;
			J &= ~VBA_BUTTON_B;
		} else
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






/****************************************************************************
 * Visual Boy Advance GX
 *
 * Carl Kenner May 2009
 *
 * inputstarwars.cpp
 *
 * Wii/Gamecube controls for Star Wars games
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

u32 LegoStarWars1Input(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DPadWASD(pad);
	// Rumble when they lose health!
	u8 Health = 0;
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

#ifdef HW_RVL
	if (DownUsbKeys[KB_ENTER]) J |= VBA_BUTTON_START; // start
	if (DownUsbKeys[KB_K] || DownUsbKeys[KB_LCTRL]) J |= VBA_BUTTON_SELECT; // change chars
	if (DownUsbKeys[KB_U]) J |= VBA_BUTTON_A; // jump
	if (DownUsbKeys[KB_H]) J |= VBA_BUTTON_B; // attack
	if (DownUsbKeys[KB_J]) J |= VBA_BUTTON_R; // force power, special ability
	if (DownUsbKeys[KB_I]) J |= VBA_BUTTON_L; // build, use force (supposed to be J too)
	if (DownUsbKeys[KB_SPACE]) J |= VBA_SPEED; // fast forward

	WPADData * wp = WPAD_Data(pad);

	// Start/Select
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;

	if (wp->exp.type == WPAD_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == WPAD_EXP_NUNCHUK) {
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
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		J |= DecodeClassic(pad);
	} else J |= DecodeWiimote(pad);
#endif
	return J;
}

u32 LegoStarWars2Input(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DPadWASD(pad);
	// Rumble when they lose health!
	u8 Health = 0;
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

#ifdef HW_RVL
	if (DownUsbKeys[KB_ENTER]) J |= VBA_BUTTON_START; // start
	if (DownUsbKeys[KB_K] || DownUsbKeys[KB_LCTRL]) J |= VBA_BUTTON_SELECT; // change chars
	if (DownUsbKeys[KB_U]) J |= VBA_BUTTON_A; // jump
	if (DownUsbKeys[KB_H]) J |= VBA_BUTTON_B; // attack
	if (DownUsbKeys[KB_J]) J |= VBA_BUTTON_L; // force power, special ability
	if (DownUsbKeys[KB_I]) J |= VBA_BUTTON_R; // build, use force (supposed to be J too)
	if (DownUsbKeys[KB_SPACE]) J |= VBA_SPEED; // fast forward

	WPADData * wp = WPAD_Data(pad);

	// Start/Select
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;

	if (wp->exp.type == WPAD_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == WPAD_EXP_NUNCHUK) {
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

	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		J |= DecodeClassic(pad);
	}
#endif
	return J;
}

u32 SWObiWanInput(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeKeyboard(pad);
	// Rumble when they lose health!
	u8 Health = gbReadMemory(0xCFF2);
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	// Start/Select
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	if (wp->exp.type == WPAD_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		// use the force
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) {
			gbWriteMemory(0xCFF1, 2);
			J |= VBA_BUTTON_A;
		}
		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Shoot
		if (wp->btns_h & WPAD_BUTTON_B) {
			gbWriteMemory(0xCFF1, 0);
			J |= VBA_BUTTON_A;
		}
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			gbWriteMemory(0xCFF1, 1);
			J |= VBA_BUTTON_A;
		}
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		J |= DecodeClassic(pad);
	}
#endif
	return J;
}

u32 SWEpisode2Input(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeKeyboard(pad);
	// Rumble when they lose health!
	u8 Health = CPUReadByte(0x3002fb3); 
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	// Start/Select
	if (wp->btns_h & WPAD_BUTTON_PLUS)
		J |= VBA_BUTTON_START;
	if (wp->btns_h & WPAD_BUTTON_MINUS)
		J |= VBA_BUTTON_SELECT;

	if (wp->exp.type == WPAD_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Shoot (N/A)
		if (wp->btns_h & WPAD_BUTTON_B) {
			J |= VBA_BUTTON_A;
		}
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_A;
		}
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
		// CAKTODO
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_L;
		}
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) {
			J |= VBA_BUTTON_R;
		}
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		J |= DecodeClassic(pad);
	}
#endif
	return J;
}

u32 SWEpisode3Input(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeKeyboard(pad);
	// Rumble when they lose health!
	u8 Health = 0;//CPUReadByte(0x3002fb3); 
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	if (wp->exp.type == WPAD_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Shoot (N/A)
		if (wp->btns_h & WPAD_BUTTON_B) {
			J |= VBA_BUTTON_B;
		}
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_B;
		}
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
		// CAKTODO
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_L;
		}
		// Force
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) {
			J |= VBA_BUTTON_R;
		}
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
#endif
	return J;
}

u32 SWJediPowerBattlesInput(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeKeyboard(pad);
	// Rumble when they lose health!
	u8 Health = 0;//CPUReadByte(0x3002fb3); 
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	if (wp->exp.type == WPAD_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_B;
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_A;
		}
		// Shoot ?
		if (wp->btns_h & WPAD_BUTTON_B) {
			J |= VBA_BUTTON_A;
		}
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
		// Block
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_R;
		}
		// Force
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) {
			J |= VBA_BUTTON_L;
		}
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
#endif
	return J;
}

u32 SWTrilogyInput(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeKeyboard(pad);
	// Rumble when they lose health!
	u8 Health = 0;//CPUReadByte(0x3002fb3); 
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	if (wp->exp.type == WPAD_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_B;
		}
		// Shoot
		if (wp->btns_h & WPAD_BUTTON_B) {
			J |= VBA_BUTTON_B;
		}
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
		// Block
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_L;
		}
		// Force
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) {
			J |= VBA_BUTTON_R;
		}
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
#endif
	return J;
}

u32 SWEpisode4Input(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeKeyboard(pad);
	// Rumble when they lose health!
	u8 Health = 0;
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	if (wp->exp.type == WPAD_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_B;
		}
		// Shoot / run
		if (wp->btns_h & WPAD_BUTTON_B) {
			J |= VBA_BUTTON_B;
		}
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
		// Block
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_B;
		}
		// Force
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) {
			
		}
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
#endif
	return J;
}

u32 SWEpisode5Input(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeKeyboard(pad);
	// Rumble when they lose health!
	u8 Health = 0;
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	if (wp->exp.type == WPAD_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_B;
		}
		// Shoot / run
		if (wp->btns_h & WPAD_BUTTON_B) {
			J |= VBA_BUTTON_B;
		}
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
		// Block
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_B;
		}
		// Force
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) {
			
		}
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
#endif
	return J;
}

u32 SWEpisode6Input(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeKeyboard(pad);
	// Rumble when they lose health!
	u8 Health = 0;
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	if (wp->exp.type == WPAD_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		// Jump
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_A;
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_B;
		}
		// Shoot / run
		if (wp->btns_h & WPAD_BUTTON_B) {
			J |= VBA_BUTTON_B;
		}
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
		// Block
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_B;
		}
		// Force
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) {
			
		}
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
#endif
	return J;
}

u32 SWYodaStoriesInput(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeKeyboard(pad);
	// Rumble when they lose health!
	u8 Health = 0;//gbReadMemory(0xD58A); // actually health bar progress
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	if (wp->exp.type == WPAD_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		// Drag object, get out of dialog
		if (wp->btns_h & WPAD_BUTTON_A)
			J |= VBA_BUTTON_B;
		
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			// should change weapon here
			J |= VBA_BUTTON_A;
		}
		// Shoot / run
		if (wp->btns_h & WPAD_BUTTON_B) {
			// should change weapon here
			J |= VBA_BUTTON_A;
		}
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
		// Force
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) {
			// should change weapon here
			J |= VBA_BUTTON_A;
		}
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
#endif
	return J;
}

u32 SWNDAInput(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeKeyboard(pad);
	// Rumble when they lose health!
	u8 Health = 0;
	static u8 OldHealth = 0;
	if (Health < OldHealth)
		systemGameRumble(6);
	OldHealth = Health;

#ifdef HW_RVL
	WPADData * wp = WPAD_Data(pad);

	if (wp->exp.type == WPAD_EXP_NONE) {
		J |= DecodeWiimote(pad);
	} else if (wp->exp.type == WPAD_EXP_NUNCHUK) {
		// Jump attack, the only kind of jumping in this game
		// Note, this only works if you press B first, then A
		if (wp->btns_h & WPAD_BUTTON_A) {
			J &= ~(VBA_DOWN | VBA_LEFT | VBA_RIGHT);
			J |= VBA_BUTTON_A | VBA_UP;
		}
		// Light saber
		if (fabs(wp->gforce.x)> 1.5 ) {
			J |= VBA_BUTTON_A;
		}
		// Activate light saber
		if (wp->btns_h & WPAD_BUTTON_B) {
			J |= VBA_BUTTON_A;
		}
		if (wp->btns_h & WPAD_BUTTON_1 || wp->btns_h & WPAD_BUTTON_2)
			J |= VBA_SPEED;
		// Force
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_Z) {
			J |= VBA_BUTTON_R;
		}
		// Block
		if (wp->btns_h & WPAD_NUNCHUK_BUTTON_C) {
			J |= VBA_BUTTON_B;
		}
		// Change force power
		if (wp->btns_h & (WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT | WPAD_BUTTON_UP | WPAD_BUTTON_DOWN)) {
			J |= VBA_BUTTON_L;
		}
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
	} else if (wp->exp.type == WPAD_EXP_CLASSIC) {
		// Start/Select
		if (wp->btns_h & WPAD_BUTTON_PLUS)
			J |= VBA_BUTTON_START;
		if (wp->btns_h & WPAD_BUTTON_MINUS)
			J |= VBA_BUTTON_SELECT;
		J |= DecodeClassic(pad);
	}
#endif
	return J;
}

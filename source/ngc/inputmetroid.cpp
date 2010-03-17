/****************************************************************************
 * Visual Boy Advance GX
 *
 * Carl Kenner Febuary 2009
 *
 * gameinput.cpp
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
#include <wiiuse/wpad.h>

#include "vba.h"
#include "button_mapping.h"
#include "audio.h"
#include "video.h"
#include "input.h"
#include "gameinput.h"
#include "vbasupport.h"
#include "gba/GBA.h"
#include "gba/bios.h"
#include "gba/GBAinline.h"

u32 MetroidZeroInput(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeClassic(pad);
	u8 BallState = CPUReadByte(0x30015df); // 0 = stand, 1 = crouch, 2 = ball
	u16 Health = CPUReadByte(0x3001536); // 0 = stand, 1 = crouch, 2 = ball
	static u16 OldHealth = 0;

	// Rumble when they lose health!
	if (Health < OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;
#ifdef HW_RVL
	static int AimCount = 0;
#endif
	static int MissileCount = 0;

	if (BallState == 2) // Can't exit morph ball without pressing C!
		J &= ~VBA_UP;
	if (BallState == 1) // Can't enter morph ball without pressing C!
		J &= ~VBA_DOWN;

	bool MorphBallButton = 0;
#ifdef HW_RVL
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
	if (wp->orient.pitch < -45 && BallState !=2) {
		J |= VBA_UP;
		AimCount = 0;
	} else if (wp->orient.pitch < -22 && BallState !=2) {
		if (AimCount>=0) AimCount = -1;
	} else if (wp->orient.pitch> 45 && BallState ==0) {
		if (AimCount<10) AimCount=10;
	} else if (wp->orient.pitch> 22 && BallState !=2) {
		if (AimCount<=0 || AimCount>=10) AimCount = 1;
	} else {
		AimCount=0;
	}

	// Morph Ball
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_d & WPAD_NUNCHUK_BUTTON_C)) {
		MorphBallButton = true;
	}
	// Missile
	if (wp->btns_h & WPAD_BUTTON_DOWN) {
		MissileCount = 1;
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


#endif
	{
		u32 gc = PAD_ButtonsHeld(pad);
		u32 pressed = PAD_ButtonsDown(pad);
		if (pressed & PAD_BUTTON_X) MorphBallButton = true;
		if (gc & PAD_BUTTON_A) J |= VBA_BUTTON_B; // shoot beam
		if (gc & PAD_BUTTON_B) J |= VBA_BUTTON_A; // jump
		if (gc & PAD_BUTTON_Y) MissileCount = 1; // missile
		if (gc & PAD_TRIGGER_Z) J |= VBA_BUTTON_START; // map
		if (gc & PAD_TRIGGER_L) J |= VBA_BUTTON_L; // aim up/down
		if (gc & PAD_BUTTON_START) J |= VBA_BUTTON_SELECT; // toggle super missiles
		if (gc & PAD_TRIGGER_R) J |= VBA_BUTTON_R; // ???
	}
	switch (MissileCount) {
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
	if (MorphBallButton) {
		if (BallState == 2) { // ball
			Morph = -1;
		} else if (BallState == 1) {
			Morph = 2;
		} else {
			Morph = 1;
		}
	}
	switch (Morph) {
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

	return J;
}

u32 MetroidFusionInput(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeClassic(pad);
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

#ifdef HW_RVL
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
	if (wp->orient.pitch < -45 && BallState !=5) {
		J |= VBA_UP;
		AimCount = 0;
	} else if (wp->orient.pitch < -22 && BallState !=5) {
		if (AimCount>=0) AimCount = -1;
	} else if (wp->orient.pitch> 45 && BallState !=5) {
		if (AimCount<10) AimCount=10;
	} else if (wp->orient.pitch> 22 && BallState !=5) {
		if (AimCount<=0 || AimCount>=10) AimCount = 1;
	} else {
		AimCount=0;
	}

	// Morph Bdall
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_d & WPAD_NUNCHUK_BUTTON_C)) {
		if (BallState == 5) { // ball
			Morph = -1;
		} else if (BallState == 2) {
			Morph = 2;
		} else {
			Morph = 1;
		}
	}
	// Missile
	if (wp->btns_h & WPAD_BUTTON_DOWN) {
		MissileCount = 1;
	}
#endif

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

	switch (Morph) {
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

	return J;
}

u32 Metroid1Input(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeClassic(pad);
	u8 BallState = CPUReadByte(0x3007500); // 3 = ball, other = stand
#ifdef HW_RVL
	u8 MissileState = CPUReadByte(0x300730E); // 1 = missile, 0 = beam
#endif
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

#ifdef HW_RVL
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
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_d & WPAD_NUNCHUK_BUTTON_C)) {
		if (BallState == 3) { // ball
			Morph = -1;
		} else {
			Morph = 2;
		}
	}
	// Missile
	if (wp->btns_h & WPAD_BUTTON_DOWN) {
		if (!(MissileState))
			J |= VBA_BUTTON_SELECT;
		else
			J |= VBA_BUTTON_B;
	} if (wp->btns_u & WPAD_BUTTON_DOWN) {
		if (MissileState)
			J |= VBA_BUTTON_SELECT;
	}
#endif
	switch (Morph) {
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

	return J;
}

u32 Metroid2Input(unsigned short pad) {
	u32 J = StandardMovement(pad) | DecodeGamecube(pad) | DecodeClassic(pad);
	u8 BallState = gbReadMemory(0xD020); // 4 = crouch, 5 = ball, other = stand
#ifdef HW_RVL
	u8 MissileState = gbReadMemory(0xD04D); // 8 = missile hatch open, 0 = missile closed
#endif
	u8 Health = gbReadMemory(0xD051); // Binary Coded Decimal
	static u8 OldHealth = 0;

	// Rumble when they lose (or gain) health! (since I'm not checking energy tanks)
	if (Health != OldHealth)
		systemGameRumble(20);
	OldHealth = Health;

	static int Morph = 0;
#ifdef HW_RVL
	static int AimCount = 0;
#endif

	if (BallState == 5) // Can't exit morph ball without pressing C!
		J &= ~VBA_UP;
	if (BallState == 4) // Can't enter morph ball without pressing C!
		J &= ~VBA_DOWN;

#ifdef HW_RVL
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
	if (wp->orient.pitch < -45 && BallState !=5) {
		J |= VBA_UP;
		AimCount = 0;
	} else if (wp->orient.pitch> 45 && BallState !=5 && BallState !=4) {
		//if (AimCount<10) AimCount=10;
	} else {
		AimCount=0;
	}

	// Morph Ball
	if ((wp->exp.type == WPAD_EXP_NUNCHUK) && (wp->btns_d & WPAD_NUNCHUK_BUTTON_C)) {
		if (BallState == 5) { // ball
			Morph = -1;
		} else if (BallState == 4) {
			Morph = 2;
		} else {
			Morph = 1;
		}
	}
	// Missile
	if (wp->btns_h & WPAD_BUTTON_DOWN) {
		if (!(MissileState & 8))
			J |= VBA_BUTTON_SELECT;
		else
			J |= VBA_BUTTON_B;
	}
	if (wp->btns_u & WPAD_BUTTON_DOWN) {
		if (MissileState & 8)
			J |= VBA_BUTTON_SELECT;
	}
#endif

	switch (Morph) {
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

	return J;
}


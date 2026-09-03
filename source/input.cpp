/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * input.cpp
 *
 * Wii/Gamecube controller management
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vbagx.h"
#include "button_mapping.h"
#include "video.h"
#include "input.h"
#include "gameinput.h"
#include "libgui/Gui.h"
#include "vbasupport.h"
#include "vba/gba/GBA.h"
#include "vba/gba/bios.h"
#include "vba/gba/GBAinline.h"

#define ANALOG_SENSITIVITY 30

int playerMapping[4] = {0,1,2,3};

static unsigned int vbapadmap[MAXJP]; // VBA controller buttons
uint32_t btnmap[GUI_HW_MAX][MAXJP]; // button mapping

void ResetControls(int wiiCtrl)
{
	int i;

	// VBA controller buttons
	i=0;
	vbapadmap[i++] = VBA_BUTTON_B;
	vbapadmap[i++] = VBA_BUTTON_A;
	vbapadmap[i++] = VBA_BUTTON_SELECT;
	vbapadmap[i++] = VBA_BUTTON_START;
	vbapadmap[i++] = VBA_UP;
	vbapadmap[i++] = VBA_DOWN;
	vbapadmap[i++] = VBA_LEFT;
	vbapadmap[i++] = VBA_RIGHT;
	vbapadmap[i++] = VBA_BUTTON_L;
	vbapadmap[i++] = VBA_BUTTON_R;

	/*** Gamecube controller Padmap ***/
	if(wiiCtrl == GUI_HW_GAMECUBE || wiiCtrl == -1)
	{
		i=0;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_B;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_A;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_TRIGGER_ZR; // GC Z button
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_PLUS;   // GC Start
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_UP;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_DOWN;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_LEFT;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_BTN_RIGHT;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_TRIGGER_L;
		btnmap[GUI_HW_GAMECUBE][i++] = GUI_TRIGGER_R;
	}

	/*** Wiimote Padmap (Sideways) ***/
	if(wiiCtrl == GUI_HW_WIIMOTE || wiiCtrl == -1)
	{
		i=0;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_1;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_2;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_MINUS;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_PLUS;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_RIGHT;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_LEFT;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_UP;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_DOWN;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_B;
		btnmap[GUI_HW_WIIMOTE][i++] = GUI_BTN_A;
	}

	/*** Classic Controller Padmap ***/
	if(wiiCtrl == GUI_HW_CLASSIC || wiiCtrl == -1)
	{
		i=0;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_Y;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_B;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_MINUS;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_PLUS;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_UP;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_DOWN;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_LEFT;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_BTN_RIGHT;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_TRIGGER_L;
		btnmap[GUI_HW_CLASSIC][i++] = GUI_TRIGGER_R;
	}

	/*** Nunchuk + Wiimote Padmap ***/
	if(wiiCtrl == GUI_HW_NUNCHUK || wiiCtrl == -1)
	{
		i=0;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_TRIGGER_L;  // C mapped to L
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_TRIGGER_ZL; // Z mapped to ZL
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_MINUS;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_PLUS;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_UP;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_DOWN;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_LEFT;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_RIGHT;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_2;
		btnmap[GUI_HW_NUNCHUK][i++] = GUI_BTN_1;
	}
	
	/*** Wii U Pro Controller Padmap ***/
	if(wiiCtrl == GUI_HW_WUPC || wiiCtrl == -1)
	{
		i=0;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_Y;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_B;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_MINUS;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_PLUS;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_UP;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_DOWN;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_LEFT;
		btnmap[GUI_HW_WUPC][i++] = GUI_BTN_RIGHT;
		btnmap[GUI_HW_WUPC][i++] = GUI_TRIGGER_L;
		btnmap[GUI_HW_WUPC][i++] = GUI_TRIGGER_R;
	}
	
	/*** Wii U Gamepad (DRC) Padmap ***/
	if(wiiCtrl == GUI_HW_DRC || wiiCtrl == -1)
	{
		i=0;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_Y;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_B;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_MINUS;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_PLUS;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_UP;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_DOWN;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_LEFT;
		btnmap[GUI_HW_DRC][i++] = GUI_BTN_RIGHT;
		btnmap[GUI_HW_DRC][i++] = GUI_TRIGGER_L;
		btnmap[GUI_HW_DRC][i++] = GUI_TRIGGER_R;
	}
}

void systemPossibleCartridgeRumble(bool rumbleOn) {
	if (userInput[0]) userInput[0]->setContinuousRumble(rumbleOn);
}

void systemCartridgeRumble(bool rumbleOn) {
	if (userInput[0]) userInput[0]->setContinuousRumble(rumbleOn);
}

void systemGameRumble(int rumbleForFrames) {
	if (userInput[0]) userInput[0]->ensureGameRumble(rumbleForFrames);
}

void systemGameRumbleOnlyFor(int onlyRumbleForFrames) {
	if (userInput[0]) userInput[0]->setGameRumble(onlyRumbleForFrames);
}

uint32_t StandardMovement(unsigned short chan)
{
	if (!userInput[chan]) return 0;
	const GuiInputPadData& pad = userInput[chan]->getPadData();
	uint32_t J = 0;
	
	float sensitivity = (float)ANALOG_SENSITIVITY / 128.0f;
	if (pad.stickY > sensitivity) J |= VBA_UP;
	else if (pad.stickY < -sensitivity) J |= VBA_DOWN;
	if (pad.stickX < -sensitivity) J |= VBA_LEFT;
	else if (pad.stickX > sensitivity) J |= VBA_RIGHT;

	return J;
}

uint32_t StandardDPad(unsigned short pad)
{
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = 0;
	if (data.buttons_h & GUI_BTN_UP) J |= VBA_UP;
	if (data.buttons_h & GUI_BTN_DOWN) J |= VBA_DOWN;
	if (data.buttons_h & GUI_BTN_LEFT) J |= VBA_LEFT;
	if (data.buttons_h & GUI_BTN_RIGHT) J |= VBA_RIGHT;
	return J;
}

uint32_t StandardSideways(unsigned short pad)
{
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = 0;

	// Read directly from isolated Wiimote state
	uint32_t wp = data.hw_buttons_h[GUI_HW_WIIMOTE];

	if (wp & GUI_BTN_RIGHT) J |= VBA_UP;
	else if (wp & GUI_BTN_LEFT) J |= VBA_DOWN;
	if (wp & GUI_BTN_UP) J |= VBA_LEFT;
	else if (wp & GUI_BTN_DOWN) J |= VBA_RIGHT;

	if (wp & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (wp & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	if (wp & GUI_BTN_1) J |= VBA_BUTTON_B;
	if (wp & GUI_BTN_2) J |= VBA_BUTTON_A;

	if (cartridgeType == CARTRIDGE_GBA) {
		if (wp & GUI_BTN_A) J |= VBA_BUTTON_R;
		if (wp & GUI_BTN_B) J |= VBA_BUTTON_L;
	} else {
		if ((wp & GUI_BTN_B) || (wp & GUI_BTN_A)) J |= VBA_SPEED;
	}
	return J;
}

uint32_t StandardClassic(unsigned short pad)
{
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();
	uint32_t J = 0;

	// Read isolated Classic Controller state
	uint32_t wp = data.hw_buttons_h[GUI_HW_CLASSIC];

	if (wp & GUI_BTN_RIGHT) J |= VBA_RIGHT;
	else if (wp & GUI_BTN_LEFT) J |= VBA_LEFT;
	if (wp & GUI_BTN_UP) J |= VBA_UP;
	else if (wp & GUI_BTN_DOWN) J |= VBA_DOWN;

	if (wp & GUI_BTN_PLUS) J |= VBA_BUTTON_START;
	if (wp & GUI_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	if ((wp & GUI_TRIGGER_L) || (wp & GUI_TRIGGER_ZL)) J |= VBA_BUTTON_L;
	if ((wp & GUI_TRIGGER_R) || (wp & GUI_TRIGGER_ZR)) J |= VBA_BUTTON_R;

	if (wp & GUI_BTN_A) J |= VBA_BUTTON_A;
	if (wp & GUI_BTN_B) J |= VBA_BUTTON_B;
	if ((wp & GUI_BTN_Y) || (wp & GUI_BTN_X)) J |= VBA_SPEED;

	return J;
}

/****************************************************************************
 * DecodeJoy
 *
 * Reads the STATE (not changes) from a controller and reports
 * this STATE (not changes) to VBA
 ****************************************************************************/
static uint32_t DecodeJoy(unsigned short pad)
{
	if (!userInput[pad]) return 0;
	const GuiInputPadData& data = userInput[pad]->getPadData();

	CursorX = data.cursor_x;
	CursorY = data.cursor_y;
	CursorValid = data.validPointer;

	// check for games that should have special Wii controls
	if (GCSettings.WiiControls) {
		switch (RomIdCode & 0xFFFFFF) {
			// Zelda
			case ZELDA1:
				return Zelda1Input(pad);
			case ZELDA2:
				return Zelda2Input(pad);
			case ALINKTOTHEPAST:
				return ALinkToThePastInput(pad);
			case LINKSAWAKENING:
				return LinksAwakeningInput(pad);
			case ORACLEOFAGES:
				return OracleOfAgesInput(pad);
			case ORACLEOFSEASONS:
				return OracleOfSeasonsInput(pad);
			case MINISHCAP:
				return MinishCapInput(pad);

			// Metroid
			case METROID0:
				return MetroidZeroInput(pad);
			case METROID1:
				return Metroid1Input(pad);
			case METROID2:
				return Metroid2Input(pad);
			case METROID4:
				return MetroidFusionInput(pad);

			// TMNT
			case TMNT1:
				return TMNT1Input(pad);
			case TMNT2:
				return TMNT2Input(pad);
			case TMNT3:
				return TMNT3Input(pad);
			case TMNTGBA:
				return TMNTGBAInput(pad);
			case TMNTGBA2:
				return TMNTGBA2Input(pad);
			case TMNT:
				return TMNTInput(pad);

			// Medal Of Honor
			case MOHUNDERGROUND:
				return MohUndergroundInput(pad);
			case MOHINFILTRATOR:
				return MohInfiltratorInput(pad);

			// Harry Potter
			case HARRYPOTTER1GBC:
				return HarryPotter1GBCInput(pad);
			case HARRYPOTTER2GBC:
				return HarryPotter2GBCInput(pad);
			case HARRYPOTTER1:
				return HarryPotter1Input(pad);
			case HARRYPOTTER2:
				return HarryPotter2Input(pad);
			case HARRYPOTTER3:
				return HarryPotter3Input(pad);
			case HARRYPOTTER4:
				return HarryPotter4Input(pad);
			case HARRYPOTTER5:
				return HarryPotter5Input(pad);

			// Mario
			case MARIO1CLASSIC:
			case MARIO2CLASSIC:
				return Mario1ClassicInput(pad);
			case MARIO1DX:
				return Mario1DXInput(pad);
			case MARIO2ADV:
				return Mario2Input(pad);
			case MARIO3ADV:
				return Mario3Input(pad);
			case MARIOWORLD:
				return MarioWorldInput(pad);
			case YOSHIISLAND:
				return YoshiIslandInput(pad);
			case MARIOLAND1:
				return MarioLand1Input(pad);
			case MARIOLAND2:
				return MarioLand2Input(pad);
			case YOSHIUG:
				return UniversalGravitationInput(pad);

			// Mario Kart
			case MARIOKART:
				return MarioKartInput(pad);

			// Lego Star Wars
			case LSW1:
				return LegoStarWars1Input(pad);
			case LSW2:
				return LegoStarWars2Input(pad);

			// Star Wars
			case SWOBIWAN:
				return SWObiWanInput(pad);
			case SWJPB:
				return SWJediPowerBattlesInput(pad);
			case SWEP2:
				return SWEpisode2Input(pad);
			case SWEP3:
				return SWEpisode3Input(pad);
			case SWEP4:
				return SWEpisode4Input(pad);
			case SWEP5:
				return SWEpisode5Input(pad);
			case SWEP6:
				return SWEpisode6Input(pad);
			case SWTRILOGY:
				return SWTrilogyInput(pad);
			case SWNDA:
				return SWNDAInput(pad);
			case SWYODA:
				return SWYodaStoriesInput(pad);

			// Mortal Kombat
			case MK1:
				return MK1Input(pad);
			case MK12:
				return MK12Input(pad);
			case MK2:
				return MK2Input(pad);
			case MK3:
				return MK3Input(pad);
			case MK4:
				return MK4Input(pad);
			case MKA:
				return MKAInput(pad);
			case MKDA:
				return MKDAInput(pad);
			case MKTE:
				return MKTEInput(pad);

			// WarioWare
			case TWISTED:
				return TwistedInput(pad);

			// Kirby
			case KIRBYTNT:
			case KIRBYTNTJ:
				return KirbyTntInput(pad);

			// Boktai
			case BOKTAI1:
				return BoktaiInput(pad);
			case BOKTAI2:
			case BOKTAI3:
				return Boktai2Input(pad);

			// One Piece
			case ONEPIECE:
				return OnePieceInput(pad);

			// Lord of the Rings
			case HOBBIT:
				return HobbitInput(pad);
			case LOTR1:
				return FellowshipOfTheRingInput(pad);
			case LOTR2:
			case LOTR3:
				return ReturnOfTheKingInput(pad);

			// Castlevania
			case CVADVENTURE:
				return CastlevaniaAdventureInput(pad);
			case CVBELMONT:
				return CastlevaniaBelmontInput(pad);
			case CVLEGENDS:
				return CastlevaniaLegendsInput(pad);
			case CVCIRCLEMOON:
			case CVHARMONY:
			case CVARIA:
				return CastlevaniaCircleMoonInput(pad);

			case KIDDRACULA:
				return KidDraculaInput(pad);
		}
	}

	// Get baseline movement (translates unified Analog Sticks to VBA directions)
	uint32_t J = StandardMovement(pad);

	// Evaluate Turbo (C-Stick Right / Right Stick Right)
	if (GCSettings.TurboModeEnabled)
	{
		if (data.substickX > 0.55f) {
			J |= VBA_SPEED;
		}
	}

	// Evaluate Standard Buttons
	for (int i = 0; i < 10; ++i)
	{
		bool button_pressed = false;

		// Check if ANY connected hardware matches the mapping
		for (uint32_t hw = 0; hw < GUI_HW_MAX; hw++)
		{
			if (!data.hw_connected[hw]) continue;
			uint32_t mapped_btn = btnmap[hw][i];

			if (data.hw_buttons_h[hw] & mapped_btn) {
				button_pressed = true;
				break;
			}
		}

		if (button_pressed) {
			J |= vbapadmap[i];
		}
	}

	return J;
}

bool isMenuRequested()
{
	for(int i=0; i<4; i++) {
		if (!userInput[i]) continue;
		const GuiInputPadData& pad = userInput[i]->getPadData();

		bool rightStickLeft = (pad.substickX < -0.55f);
		bool homePressed = (pad.buttons_h & GUI_BTN_HOME);
		bool lPlusRPlusStart = (pad.buttons_h & GUI_TRIGGER_L) &&
							   (pad.buttons_h & GUI_TRIGGER_R) &&
							   (pad.buttons_h & GUI_BTN_PLUS);

		if (rightStickLeft || lPlusRPlusStart || homePressed)
		{
			return true; 
		}
	}
	return false;
}

static int GetPlayerChan(int pad)
{
	for(int i=3; i >= 0; i--) {
		if(playerMapping[i] == pad) {
			return i;
		}
	}
	return pad;
}

uint32_t GetJoy(int pad)
{
	// request to go back to menu
	if (isMenuRequested())
	{
		MenuRequested = true;
		return 0;
	}

	int chan = GetPlayerChan(pad);

	uint32_t J = DecodeJoy(chan);
	// don't allow up+down or left+right
	if ((J & 48) == 48)
		J &= ~16;
	if ((J & 192) == 192)
		J &= ~128;

	return J;
}

/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * input.cpp
 *
 * Controller management
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
uint32_t btnmap[INPUT_HW_MAX][MAXJP]; // button mapping

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
	if(wiiCtrl == INPUT_HW_GAMECUBE || wiiCtrl == -1)
	{
		i=0;
		btnmap[INPUT_HW_GAMECUBE][i++] = INPUT_BTN_B;
		btnmap[INPUT_HW_GAMECUBE][i++] = INPUT_BTN_A;
		btnmap[INPUT_HW_GAMECUBE][i++] = INPUT_TRIGGER_ZR; // GC Z button
		btnmap[INPUT_HW_GAMECUBE][i++] = INPUT_BTN_PLUS;   // GC Start
		btnmap[INPUT_HW_GAMECUBE][i++] = INPUT_BTN_UP;
		btnmap[INPUT_HW_GAMECUBE][i++] = INPUT_BTN_DOWN;
		btnmap[INPUT_HW_GAMECUBE][i++] = INPUT_BTN_LEFT;
		btnmap[INPUT_HW_GAMECUBE][i++] = INPUT_BTN_RIGHT;
		btnmap[INPUT_HW_GAMECUBE][i++] = INPUT_TRIGGER_L;
		btnmap[INPUT_HW_GAMECUBE][i++] = INPUT_TRIGGER_R;
	}

	/*** Wiimote Padmap (Sideways) ***/
	if(wiiCtrl == INPUT_HW_WIIMOTE || wiiCtrl == -1)
	{
		i=0;
		btnmap[INPUT_HW_WIIMOTE][i++] = INPUT_BTN_1;
		btnmap[INPUT_HW_WIIMOTE][i++] = INPUT_BTN_2;
		btnmap[INPUT_HW_WIIMOTE][i++] = INPUT_BTN_MINUS;
		btnmap[INPUT_HW_WIIMOTE][i++] = INPUT_BTN_PLUS;
		btnmap[INPUT_HW_WIIMOTE][i++] = INPUT_BTN_RIGHT;
		btnmap[INPUT_HW_WIIMOTE][i++] = INPUT_BTN_LEFT;
		btnmap[INPUT_HW_WIIMOTE][i++] = INPUT_BTN_UP;
		btnmap[INPUT_HW_WIIMOTE][i++] = INPUT_BTN_DOWN;
		btnmap[INPUT_HW_WIIMOTE][i++] = INPUT_BTN_B;
		btnmap[INPUT_HW_WIIMOTE][i++] = INPUT_BTN_A;
	}

	/*** Classic Controller Padmap ***/
	if(wiiCtrl == INPUT_HW_CLASSIC || wiiCtrl == -1)
	{
		i=0;
		btnmap[INPUT_HW_CLASSIC][i++] = INPUT_BTN_Y;
		btnmap[INPUT_HW_CLASSIC][i++] = INPUT_BTN_B;
		btnmap[INPUT_HW_CLASSIC][i++] = INPUT_BTN_MINUS;
		btnmap[INPUT_HW_CLASSIC][i++] = INPUT_BTN_PLUS;
		btnmap[INPUT_HW_CLASSIC][i++] = INPUT_BTN_UP;
		btnmap[INPUT_HW_CLASSIC][i++] = INPUT_BTN_DOWN;
		btnmap[INPUT_HW_CLASSIC][i++] = INPUT_BTN_LEFT;
		btnmap[INPUT_HW_CLASSIC][i++] = INPUT_BTN_RIGHT;
		btnmap[INPUT_HW_CLASSIC][i++] = INPUT_TRIGGER_L;
		btnmap[INPUT_HW_CLASSIC][i++] = INPUT_TRIGGER_R;
	}

	/*** Nunchuk + Wiimote Padmap ***/
	if(wiiCtrl == INPUT_HW_NUNCHUK || wiiCtrl == -1)
	{
		i=0;
		btnmap[INPUT_HW_NUNCHUK][i++] = INPUT_TRIGGER_L;  // C mapped to L
		btnmap[INPUT_HW_NUNCHUK][i++] = INPUT_TRIGGER_ZL; // Z mapped to ZL
		btnmap[INPUT_HW_NUNCHUK][i++] = INPUT_BTN_MINUS;
		btnmap[INPUT_HW_NUNCHUK][i++] = INPUT_BTN_PLUS;
		btnmap[INPUT_HW_NUNCHUK][i++] = INPUT_BTN_UP;
		btnmap[INPUT_HW_NUNCHUK][i++] = INPUT_BTN_DOWN;
		btnmap[INPUT_HW_NUNCHUK][i++] = INPUT_BTN_LEFT;
		btnmap[INPUT_HW_NUNCHUK][i++] = INPUT_BTN_RIGHT;
		btnmap[INPUT_HW_NUNCHUK][i++] = INPUT_BTN_2;
		btnmap[INPUT_HW_NUNCHUK][i++] = INPUT_BTN_1;
	}
	
	/*** Wii U Pro Controller Padmap ***/
	if(wiiCtrl == INPUT_HW_WUPC || wiiCtrl == -1)
	{
		i=0;
		btnmap[INPUT_HW_WUPC][i++] = INPUT_BTN_Y;
		btnmap[INPUT_HW_WUPC][i++] = INPUT_BTN_B;
		btnmap[INPUT_HW_WUPC][i++] = INPUT_BTN_MINUS;
		btnmap[INPUT_HW_WUPC][i++] = INPUT_BTN_PLUS;
		btnmap[INPUT_HW_WUPC][i++] = INPUT_BTN_UP;
		btnmap[INPUT_HW_WUPC][i++] = INPUT_BTN_DOWN;
		btnmap[INPUT_HW_WUPC][i++] = INPUT_BTN_LEFT;
		btnmap[INPUT_HW_WUPC][i++] = INPUT_BTN_RIGHT;
		btnmap[INPUT_HW_WUPC][i++] = INPUT_TRIGGER_L;
		btnmap[INPUT_HW_WUPC][i++] = INPUT_TRIGGER_R;
	}
	
	/*** Wii U Gamepad (DRC) Padmap ***/
	if(wiiCtrl == INPUT_HW_DRC || wiiCtrl == -1)
	{
		i=0;
		btnmap[INPUT_HW_DRC][i++] = INPUT_BTN_Y;
		btnmap[INPUT_HW_DRC][i++] = INPUT_BTN_B;
		btnmap[INPUT_HW_DRC][i++] = INPUT_BTN_MINUS;
		btnmap[INPUT_HW_DRC][i++] = INPUT_BTN_PLUS;
		btnmap[INPUT_HW_DRC][i++] = INPUT_BTN_UP;
		btnmap[INPUT_HW_DRC][i++] = INPUT_BTN_DOWN;
		btnmap[INPUT_HW_DRC][i++] = INPUT_BTN_LEFT;
		btnmap[INPUT_HW_DRC][i++] = INPUT_BTN_RIGHT;
		btnmap[INPUT_HW_DRC][i++] = INPUT_TRIGGER_L;
		btnmap[INPUT_HW_DRC][i++] = INPUT_TRIGGER_R;
	}
}

void systemPossibleCartridgeRumble(bool rumbleOn) {
	if (controller[0]) controller[0]->setContinuousRumble(rumbleOn);
}

void systemCartridgeRumble(bool rumbleOn) {
	if (controller[0]) controller[0]->setContinuousRumble(rumbleOn);
}

void systemGameRumble(int rumbleForFrames) {
	if (controller[0]) controller[0]->ensureGameRumble(rumbleForFrames);
}

void systemGameRumbleOnlyFor(int onlyRumbleForFrames) {
	if (controller[0]) controller[0]->setGameRumble(onlyRumbleForFrames);
}

uint32_t StandardMovement(unsigned short chan)
{
	if (!controller[chan]) return 0;
	const InputPadData& pad = controller[chan]->getPadData();
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
	if (!controller[pad]) return 0;
	const InputPadData& data = controller[pad]->getPadData();
	uint32_t J = 0;
	if (data.buttons_h & INPUT_BTN_UP) J |= VBA_UP;
	if (data.buttons_h & INPUT_BTN_DOWN) J |= VBA_DOWN;
	if (data.buttons_h & INPUT_BTN_LEFT) J |= VBA_LEFT;
	if (data.buttons_h & INPUT_BTN_RIGHT) J |= VBA_RIGHT;
	return J;
}

//! The D-pad of every connected piece of hardware as directions - each in its own orientation, so a
//! Wiimote held sideways has its D-pad turned a quarter (as StandardSideways does)
static uint32_t HardwareDPad(unsigned short pad)
{
	if (!controller[pad]) return 0;
	const InputPadData& data = controller[pad]->getPadData();
	const bool sideways = controller[pad]->isSideways();
	uint32_t J = 0;

	for (uint32_t hw = 0; hw < INPUT_HW_MAX; hw++)
	{
		if (!data.hw_connected[hw]) continue;
		const uint32_t held = data.hw_buttons_h[hw];

		if (hw == INPUT_HW_WIIMOTE && sideways)
		{
			if (held & INPUT_BTN_RIGHT) J |= VBA_UP;
			if (held & INPUT_BTN_LEFT) J |= VBA_DOWN;
			if (held & INPUT_BTN_UP) J |= VBA_LEFT;
			if (held & INPUT_BTN_DOWN) J |= VBA_RIGHT;
		}
		else
		{
			if (held & INPUT_BTN_UP) J |= VBA_UP;
			if (held & INPUT_BTN_DOWN) J |= VBA_DOWN;
			if (held & INPUT_BTN_LEFT) J |= VBA_LEFT;
			if (held & INPUT_BTN_RIGHT) J |= VBA_RIGHT;
		}
	}
	return J;
}

//! The sticks and the D-pads: every way a controller can say "left"
uint32_t StandardDirections(unsigned short pad)
{
	return StandardMovement(pad) | HardwareDPad(pad);
}

//! Held buttons for the per-game "Match Wii Controls" handlers, which are written for the Wii layout:
//! A is the main action and B the secondary one (as on the Wiimote, whose B is the trigger). On the Classic
//! Controller, Pro Controller and GamePad the main action is the bottom button, B, as it was for the Classic
//! Controller in those handlers before the input refactor - so there A and B change places.
uint32_t GameButtonsHeld(const InputPadData& data)
{
	uint32_t held = 0;

	for (uint32_t hw = 0; hw < INPUT_HW_MAX; hw++)
	{
		if (!data.hw_connected[hw]) continue;
		uint32_t buttons = data.hw_buttons_h[hw];

		if (hw == INPUT_HW_CLASSIC || hw == INPUT_HW_WUPC || hw == INPUT_HW_DRC)
		{
			const uint32_t a = buttons & INPUT_BTN_A;
			const uint32_t b = buttons & INPUT_BTN_B;
			buttons &= ~(INPUT_BTN_A | INPUT_BTN_B);
			if (a) buttons |= INPUT_BTN_B;
			if (b) buttons |= INPUT_BTN_A;
		}
		held |= buttons;
	}
	return held;
}

uint32_t StandardSideways(unsigned short pad)
{
	if (!controller[pad]) return 0;
	const InputPadData& data = controller[pad]->getPadData();
	uint32_t J = 0;

	// Read directly from isolated Wiimote state
	uint32_t wp = data.hw_buttons_h[INPUT_HW_WIIMOTE];

	if (wp & INPUT_BTN_RIGHT) J |= VBA_UP;
	else if (wp & INPUT_BTN_LEFT) J |= VBA_DOWN;
	if (wp & INPUT_BTN_UP) J |= VBA_LEFT;
	else if (wp & INPUT_BTN_DOWN) J |= VBA_RIGHT;

	if (wp & INPUT_BTN_PLUS) J |= VBA_BUTTON_START;
	if (wp & INPUT_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	if (wp & INPUT_BTN_1) J |= VBA_BUTTON_B;
	if (wp & INPUT_BTN_2) J |= VBA_BUTTON_A;

	if (cartridgeType == CARTRIDGE_GBA) {
		if (wp & INPUT_BTN_A) J |= VBA_BUTTON_R;
		if (wp & INPUT_BTN_B) J |= VBA_BUTTON_L;
	} else {
		if ((wp & INPUT_BTN_B) || (wp & INPUT_BTN_A)) J |= VBA_SPEED;
	}
	return J;
}

uint32_t StandardClassic(unsigned short pad)
{
	if (!controller[pad]) return 0;
	const InputPadData& data = controller[pad]->getPadData();
	uint32_t J = 0;

	// Read isolated Classic Controller state
	uint32_t wp = data.hw_buttons_h[INPUT_HW_CLASSIC];

	if (wp & INPUT_BTN_RIGHT) J |= VBA_RIGHT;
	else if (wp & INPUT_BTN_LEFT) J |= VBA_LEFT;
	if (wp & INPUT_BTN_UP) J |= VBA_UP;
	else if (wp & INPUT_BTN_DOWN) J |= VBA_DOWN;

	if (wp & INPUT_BTN_PLUS) J |= VBA_BUTTON_START;
	if (wp & INPUT_BTN_MINUS) J |= VBA_BUTTON_SELECT;
	if ((wp & INPUT_TRIGGER_L) || (wp & INPUT_TRIGGER_ZL)) J |= VBA_BUTTON_L;
	if ((wp & INPUT_TRIGGER_R) || (wp & INPUT_TRIGGER_ZR)) J |= VBA_BUTTON_R;

	if (wp & INPUT_BTN_A) J |= VBA_BUTTON_A;
	if (wp & INPUT_BTN_B) J |= VBA_BUTTON_B;
	if ((wp & INPUT_BTN_Y) || (wp & INPUT_BTN_X)) J |= VBA_SPEED;

	return J;
}

//! A game's own "Match Wii Controls" handler, as it reads the pad
typedef uint32_t (*GameInputFn)(unsigned short pad);

//! The "Match Wii Controls" handler for the loaded game, or nullptr if it has none
static GameInputFn FindGameInput()
{
	switch (RomIdCode & 0xFFFFFF) {
		// Zelda
		case ZELDA1:
			return Zelda1Input;
		case ZELDA2:
			return Zelda2Input;
		case ALINKTOTHEPAST:
			return ALinkToThePastInput;
		case LINKSAWAKENING:
			return LinksAwakeningInput;
		case ORACLEOFAGES:
			return OracleOfAgesInput;
		case ORACLEOFSEASONS:
			return OracleOfSeasonsInput;
		case MINISHCAP:
			return MinishCapInput;

		// Metroid
		case METROID0:
			return MetroidZeroInput;
		case METROID1:
			return Metroid1Input;
		case METROID2:
			return Metroid2Input;
		case METROID4:
			return MetroidFusionInput;

		// TMNT
		case TMNT1:
			return TMNT1Input;
		case TMNT2:
			return TMNT2Input;
		case TMNT3:
			return TMNT3Input;
		case TMNTGBA:
			return TMNTGBAInput;
		case TMNTGBA2:
			return TMNTGBA2Input;
		case TMNT:
			return TMNTInput;

		// Medal Of Honor
		case MOHUNDERGROUND:
			return MohUndergroundInput;
		case MOHINFILTRATOR:
			return MohInfiltratorInput;

		// Harry Potter
		case HARRYPOTTER1GBC:
			return HarryPotter1GBCInput;
		case HARRYPOTTER2GBC:
			return HarryPotter2GBCInput;
		case HARRYPOTTER1:
			return HarryPotter1Input;
		case HARRYPOTTER2:
			return HarryPotter2Input;
		case HARRYPOTTER3:
			return HarryPotter3Input;
		case HARRYPOTTER4:
			return HarryPotter4Input;
		case HARRYPOTTER5:
			return HarryPotter5Input;

		// Mario
		case MARIO1CLASSIC:
		case MARIO2CLASSIC:
			return Mario1ClassicInput;
		case MARIO1DX:
			return Mario1DXInput;
		case MARIO2ADV:
			return Mario2Input;
		case MARIO3ADV:
			return Mario3Input;
		case MARIOWORLD:
			return MarioWorldInput;
		case YOSHIISLAND:
			return YoshiIslandInput;
		case MARIOLAND1:
			return MarioLand1Input;
		case MARIOLAND2:
			return MarioLand2Input;
		case YOSHIUG:
			return UniversalGravitationInput;

		// Mario Kart
		case MARIOKART:
			return MarioKartInput;

		// Lego Star Wars
		case LSW1:
			return LegoStarWars1Input;
		case LSW2:
			return LegoStarWars2Input;

		// Star Wars
		case SWOBIWAN:
			return SWObiWanInput;
		case SWJPB:
			return SWJediPowerBattlesInput;
		case SWEP2:
			return SWEpisode2Input;
		case SWEP3:
			return SWEpisode3Input;
		case SWEP4:
			return SWEpisode4Input;
		case SWEP5:
			return SWEpisode5Input;
		case SWEP6:
			return SWEpisode6Input;
		case SWTRILOGY:
			return SWTrilogyInput;
		case SWNDA:
			return SWNDAInput;
		case SWYODA:
			return SWYodaStoriesInput;

		// Mortal Kombat
		case MK1:
			return MK1Input;
		case MK12:
			return MK12Input;
		case MK2:
			return MK2Input;
		case MK3:
			return MK3Input;
		case MK4:
			return MK4Input;
		case MKA:
			return MKAInput;
		case MKDA:
			return MKDAInput;
		case MKTE:
			return MKTEInput;

		// WarioWare
		case TWISTED:
			return TwistedInput;

		// Kirby
		case KIRBYTNT:
		case KIRBYTNTJ:
			return KirbyTntInput;

		// Boktai
		case BOKTAI1:
			return BoktaiInput;
		case BOKTAI2:
		case BOKTAI3:
			return Boktai2Input;

		// One Piece
		case ONEPIECE:
			return OnePieceInput;

		// Lord of the Rings
		case HOBBIT:
			return HobbitInput;
		case LOTR1:
			return FellowshipOfTheRingInput;
		case LOTR2:
		case LOTR3:
			return ReturnOfTheKingInput;

		// Castlevania
		case CVADVENTURE:
			return CastlevaniaAdventureInput;
		case CVBELMONT:
			return CastlevaniaBelmontInput;
		case CVLEGENDS:
			return CastlevaniaLegendsInput;
		case CVCIRCLEMOON:
		case CVHARMONY:
		case CVARIA:
			return CastlevaniaCircleMoonInput;

		case KIDDRACULA:
			return KidDraculaInput;
	}
	return nullptr;
}

bool GameHasWiiControls()
{
	return FindGameInput() != nullptr;
}

bool WiiControlsOverrideActive()
{
	return EmuSettings.wiiControls && GameHasWiiControls();
}

/****************************************************************************
 * DecodeJoy
 *
 * Reads the STATE (not changes) from a controller and reports
 * this STATE (not changes) to VBA
 ****************************************************************************/
static uint32_t DecodeJoy(unsigned short pad)
{
	if (!controller[pad]) return 0;
	const InputPadData& data = controller[pad]->getPadData();

	CursorX = data.cursor_x;
	CursorY = data.cursor_y;
	CursorValid = data.validPointer;

	// check for games that should have special Wii controls
	if (EmuSettings.wiiControls) {
		const GameInputFn gameInput = FindGameInput();
		if (gameInput)
			return gameInput(pad);
	}

	// Get baseline movement (translates unified Analog Sticks to VBA directions)
	uint32_t J = StandardMovement(pad);

	// Evaluate Turbo (C-Stick Right / Right Stick Right)
	if (EmuSettings.turboModeEnabled)
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
		for (uint32_t hw = 0; hw < INPUT_HW_MAX; hw++)
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
		if (!controller[i]) continue;
		const InputPadData& pad = controller[i]->getPadData();

		bool rightStickLeft = (pad.substickX < -0.55f);
		bool homePressed = (pad.buttons_h & INPUT_BTN_HOME);
		bool lPlusRPlusStart = (pad.buttons_h & INPUT_TRIGGER_L) &&
							   (pad.buttons_h & INPUT_TRIGGER_R) &&
							   (pad.buttons_h & INPUT_BTN_PLUS);

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
		appRequest = AppRequest::MENU;
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

/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * wiicontrols.h
 *
 * What a game's own controls ("Match Wii Controls") do, as data, for the
 * Button Mappings screen to show while they are replacing your mappings. The
 * wording follows the README's "Special Wii Controls" section, which is the
 * intent of the original controls. For the games the README does not describe
 * (Mario Kart, Star Wars, The Lord of the Rings, Castlevania) it follows the
 * handlers in the code, which name what the controls do. Nothing here changes
 * how input is read.
 ***************************************************************************/

#pragma once

#include <stdint.h>
#include <vector>

//!Which of a game's control schemes a piece of hardware plays by
enum class WiiScheme {
	WII,		//!< Wii Remote, with or without a Nunchuk
	CLASSIC,	//!< Classic Controller, and the Wii U Pro Controller and GamePad, which are used as one
	GAMECUBE
};

//!One line of a game's controls
struct WiiAction {
	uint32_t ids;			//!< The controls it is on, as INPUT_* bits (several when they do the same thing); 0 for what is not a button (a shake, a swing)
	const char * name;		//!< A few words: "Jump"
	const char * detail;	//!< The whole story, shown as the hint while the line is selected; or null
};

struct WiiControls {
	std::vector<WiiAction> actions;
	bool mappings = false;	//!< the user's Button Mappings also apply on this controller (for what is not listed)
	bool derived = false;	//!< listed from what the code does rather than from the README: no motion, only buttons
};

//!The scheme a piece of hardware (an INPUT_HW_* value) uses
WiiScheme WiiSchemeFor(int hw);

//!The loaded game's controls on a piece of hardware (an INPUT_HW_* value): false if the game has no controls of its own
//!listed for that hardware (then `out` is empty; `out.mappings` says whether the user's Button Mappings apply there)
bool WiiControlsFor(int hw, WiiControls& out);

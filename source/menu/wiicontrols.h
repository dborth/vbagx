/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * wiicontrols.h
 *
 * What a game's own controls ("Match Wii Controls") do, as data, for the
 * Button Mappings screen to show while they are replacing your mappings. The
 * wording follows the README's "Special Wii Controls" section, which is the
 * intent of the original controls; the code that reads the controller
 * (gameinput.cpp and friends) is not consulted, and nothing here changes how
 * input is read.
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
};

//!The scheme a piece of hardware (an INPUT_HW_* value) uses
WiiScheme WiiSchemeFor(int hw);

//!The loaded game's controls on a scheme: false if nothing is written down for that game on that scheme (then `out` is empty)
bool WiiControlsFor(WiiScheme scheme, WiiControls& out);

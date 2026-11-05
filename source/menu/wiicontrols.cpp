/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * wiicontrols.cpp
 *
 * See wiicontrols.h. The wording comes from the README's "Special Wii Controls"
 * section; Mario Kart, the non-Lego Star Wars games, The Lord of the Rings and
 * Castlevania, which the README does not describe, are written from their
 * handlers (inputstarwars.cpp, gameinput.cpp), using the names the code gives
 * what the controls do. Where the code can only say which Game Boy Advance
 * button a control presses, that is what the line says ("A button").
 ***************************************************************************/

#include "wiicontrols.h"
#include "gameinput.h"
#include "drivers/InputData.h"
#include "vbasupport.h"

namespace {

// The controls, by the bits that stand for them. On the Nunchuk, C is the left trigger and Z the left Z trigger; on the
// GameCube pad Start is Plus and Z is the right Z trigger (see controllerlayout.h)
const uint32_t kA = INPUT_BTN_A, kB = INPUT_BTN_B, kX = INPUT_BTN_X, kY = INPUT_BTN_Y;
const uint32_t k1 = INPUT_BTN_1, k2 = INPUT_BTN_2;
const uint32_t kUp = INPUT_BTN_UP, kDown = INPUT_BTN_DOWN, kLeft = INPUT_BTN_LEFT, kRight = INPUT_BTN_RIGHT;
const uint32_t kDpad = kUp | kDown | kLeft | kRight;
const uint32_t kPlus = INPUT_BTN_PLUS, kMinus = INPUT_BTN_MINUS, kHome = INPUT_BTN_HOME;
const uint32_t kC = INPUT_TRIGGER_L, kZ = INPUT_TRIGGER_ZL;			// Nunchuk
const uint32_t kL = INPUT_TRIGGER_L, kR = INPUT_TRIGGER_R;
const uint32_t kZL = INPUT_TRIGGER_ZL, kZR = INPUT_TRIGGER_ZR;
const uint32_t kGcZ = INPUT_TRIGGER_ZR;									// GameCube
const uint32_t kStick = INPUT_THUMB_L, kRStick = INPUT_THUMB_R;

enum class Family {
	None, Zelda, Mario, Metroid, Turtles, MortalKombat, LegoStarWars, HarryPotter, MedalOfHonor,
	OnePiece, Boktai, Twisted, Kirby, KidDracula,
	MarioKart, StarWars,
	Isolated		// plays by what each kind of hardware reports on its own (Lord of the Rings, Castlevania)
};

struct Game {
	GameInputFn fn;
	Family family;
};

// Every game that has controls written down, by the handler it plays with
const Game kGames[] = {
	{ Zelda1Input, Family::Zelda }, { Zelda2Input, Family::Zelda }, { ALinkToThePastInput, Family::Zelda },
	{ LinksAwakeningInput, Family::Zelda }, { OracleOfAgesInput, Family::Zelda }, { OracleOfSeasonsInput, Family::Zelda },
	{ MinishCapInput, Family::Zelda },

	{ Mario1ClassicInput, Family::Mario }, { Mario1DXInput, Family::Mario }, { Mario2Input, Family::Mario },
	{ Mario3Input, Family::Mario }, { MarioWorldInput, Family::Mario }, { YoshiIslandInput, Family::Mario },
	{ MarioLand1Input, Family::Mario }, { MarioLand2Input, Family::Mario }, { UniversalGravitationInput, Family::Mario },

	{ MetroidZeroInput, Family::Metroid }, { Metroid1Input, Family::Metroid }, { Metroid2Input, Family::Metroid },
	{ MetroidFusionInput, Family::Metroid },

	{ TMNTInput, Family::Turtles }, { TMNT1Input, Family::Turtles }, { TMNT2Input, Family::Turtles }, { TMNT3Input, Family::Turtles },
	{ TMNTGBAInput, Family::Turtles }, { TMNTGBA2Input, Family::Turtles },

	{ MK1Input, Family::MortalKombat }, { MK12Input, Family::MortalKombat }, { MK2Input, Family::MortalKombat },
	{ MK3Input, Family::MortalKombat }, { MK4Input, Family::MortalKombat }, { MKAInput, Family::MortalKombat },
	{ MKDAInput, Family::MortalKombat }, { MKTEInput, Family::MortalKombat },

	{ LegoStarWars1Input, Family::LegoStarWars }, { LegoStarWars2Input, Family::LegoStarWars },

	{ HarryPotter1Input, Family::HarryPotter }, { HarryPotter2Input, Family::HarryPotter },
	{ HarryPotter3Input, Family::HarryPotter }, { HarryPotter4Input, Family::HarryPotter },
	{ HarryPotter5Input, Family::HarryPotter }, { HarryPotter1GBCInput, Family::HarryPotter },
	{ HarryPotter2GBCInput, Family::HarryPotter },

	{ MohUndergroundInput, Family::MedalOfHonor }, { MohInfiltratorInput, Family::MedalOfHonor },

	{ OnePieceInput, Family::OnePiece },
	{ BoktaiInput, Family::Boktai }, { Boktai2Input, Family::Boktai },
	{ TwistedInput, Family::Twisted }, { KirbyTntInput, Family::Kirby },
	{ KidDraculaInput, Family::KidDracula },

	{ MarioKartInput, Family::MarioKart },

	{ SWObiWanInput, Family::StarWars }, { SWJediPowerBattlesInput, Family::StarWars }, { SWEpisode2Input, Family::StarWars },
	{ SWEpisode3Input, Family::StarWars }, { SWEpisode4Input, Family::StarWars }, { SWEpisode5Input, Family::StarWars },
	{ SWEpisode6Input, Family::StarWars }, { SWTrilogyInput, Family::StarWars }, { SWNDAInput, Family::StarWars },
	{ SWYodaStoriesInput, Family::StarWars },

	{ HobbitInput, Family::Isolated }, { FellowshipOfTheRingInput, Family::Isolated }, { ReturnOfTheKingInput, Family::Isolated },
	{ CastlevaniaAdventureInput, Family::Isolated }, { CastlevaniaBelmontInput, Family::Isolated },
	{ CastlevaniaLegendsInput, Family::Isolated }, { CastlevaniaCircleMoonInput, Family::Isolated },
};

Family FamilyOf(GameInputFn fn)
{
	for(size_t i = 0; i < sizeof(kGames) / sizeof(kGames[0]); i++)
		if(kGames[i].fn == fn)
			return kGames[i].family;
	return Family::None;
}

class Builder
{
	public:
		Builder(WiiControls& out) : c(out) {}
		void add(uint32_t ids, const char * name, const char * detail = nullptr)
		{
			WiiAction a = { ids, name, detail };
			c.actions.push_back(a);
		}
	private:
		WiiControls& c;
};

bool Zelda(GameInputFn fn, WiiScheme s, Builder& b)
{
	const bool zelda2 = fn == Zelda2Input;
	const bool links = fn == LinksAwakeningInput;
	const bool oracle = fn == OracleOfAgesInput || fn == OracleOfSeasonsInput;
	const bool minish = fn == MinishCapInput;

	switch(s)
	{
		case WiiScheme::WII:
			b.add(kA, zelda2 ? "Jump" : "Action", zelda2 ? "Jump" : "Roll, talk, pick up, push, shrink or grow; also puts your sword away");
			b.add(kB, "Use item", "Uses the selected item");
			b.add(kC, "Fast-forward");
			b.add(kZ, "Target / shield", links ? "Z-targets and raises your shield (or Gust Jar). Select Bombs on the Items screen and press Z to toggle Bomb Arrows (equip the bow first)" : "Z-targets and raises your shield (or Gust Jar, if equipped)");
			b.add(k1, "Map");
			b.add(kMinus, "Items");
			b.add(kPlus, "Quest status");
			b.add(kUp, links ? "Save screen" : oracle ? "Secondary items" : "Talk to Midna", links ? "Opens the save screen" : oracle ? "Opens the secondary items" : "Talks to Midna / your hat");
			b.add(kLeft, "Item", minish ? "Uses an item directly instead of swapping" : "Another item, swappable");
			b.add(kDown, "Item", minish ? "Uses an item directly instead of swapping" : "Another item, swappable");
			b.add(kRight, "Item", minish ? "Uses an item directly instead of swapping" : "Another item, swappable");
			b.add(0, "Swing: sword", "Swing to draw and swing your sword (A puts it away; the two-handed sword must be drawn from the items menu)");
			b.add(0, "Shake Nunchuk: spin", "Shake the Nunchuk for a spin attack");
			return true;
		case WiiScheme::CLASSIC:
			b.add(kB, "Sword", "Draws and swings your sword");
			b.add(kL, "Target / shield", "Targets and raises your shield (or Gust Jar)");
			b.add(kA, "Action", "Contextual actions; also puts your sword away");
			b.add(kZL, "Fast-forward");
			b.add(kR, "Use item", "Uses the selected item");
			b.add(kRStick | kZR | kY | kX, "More items", "Three more items on the right stick directions, mirrored to ZR, Y and X");
			b.add(kPlus, "Subscreens");
			b.add(kMinus, "Map / subscreens");
			b.add(0, "Stick up: Midna", "Pushing the stick up talks to Midna / your hat");
			return true;
		case WiiScheme::GAMECUBE:
			b.add(kB, "Sword", "Draws and swings your sword (hold for a spin attack)");
			b.add(kA, "Action", "Contextual actions; also sheathes your sword");
			b.add(kL, "Target / shield", "Targets and raises your shield (or Gust Jar)");
			b.add(kR, "Pull / lift", "Pulls and lifts objects (auto-equips the bracelet / gloves); unique to this controller");
			b.add(kRStick, "Fast-forward");
			b.add(kX | kY, "Items", "Use your two equipped items (they share the B slot, except in Minish Cap, where one is in slot A)");
			b.add(kRight, "Map");
			b.add(kUp, "Items screen");
			b.add(kPlus, "Quest status");
			b.add(kGcZ, "Talk to Midna", "Talks to Midna / your hat");
			return true;
	}
	return false;
}

bool Mario(GameInputFn fn, WiiScheme s, Builder& b)
{
	// in these two a spin jump is on A or R
	const bool spinJump = fn == MarioWorldInput || fn == MarioLand2Input;

	switch(s)
	{
		case WiiScheme::WII:
			b.add(kA, "Jump");
			b.add(kB, "Shoot / run", "Shoot, run, hold on, or Yoshi's tongue (some games use B for fireballs instead)");
			b.add(kZ, "Crouch / egg", "Crouch, or lay an egg as Yoshi; press in the air to butt-stomp");
			b.add(kC, "Look around", "Hold to look around");
			b.add(kDpad, "D-pad: look / walk", "Look around or walk");
			b.add(kPlus, "Pause");
			b.add(k1, "Throw egg", "Throws an egg as Yoshi");
			b.add(kStick, "Walk / run", "Move the stick a little to walk, a lot to run");
			b.add(0, "Shake: spin / fire", "Shake to spin-attack or shoot fireballs as Fire Mario (some games use B for fireballs instead); shake to dismount Yoshi too");
			if(fn == UniversalGravitationInput)
				b.add(0, "Tilt: tilt the world", "Tilting the Wii Remote tilts the whole world, and how you move within it");
			return true;
		case WiiScheme::CLASSIC:
			b.add(kB, "Jump");
			b.add(spinJump ? (kA | kR) : kA, spinJump ? "Spin jump" : "Spin attack", spinJump ? "A or R does a spin jump" : "Spin attack");
			b.add(kX | kY, "Shoot / run", "Shoot, run, hold on, or Yoshi's tongue");
			b.add(kZL, "Crouch / egg", "Crouch, lay an egg, or butt-stomp (in some games, L)");
			b.add(spinJump ? kL : (kL | kR), "Look around", "Look around, where the game supports it");
			b.add(kZR, "Fast-forward", "8-bit Game Boy games only");
			b.add(kPlus, "Pause");
			return true;
		case WiiScheme::GAMECUBE:
			// nothing swaps on the GameCube pad: A jumps, and B, X and Y do what the Wii Remote's B does
			b.add(kA, "Jump");
			if(fn == YoshiIslandInput)
			{
				b.add(kB | kX, "Shoot / run", "Shoot, run, hold on, or Yoshi's tongue");
				b.add(kY | kR | kGcZ, "Throw egg", "Throws an egg as Yoshi");
				b.add(kL, "Crouch / egg", "Crouch, or lay an egg as Yoshi");
			}
			else if(fn == MarioWorldInput)
			{
				b.add(kB | kX | kY, "Shoot / run", "Shoot, run, hold on, or Yoshi's tongue");
				b.add(kL, "Look around", "Moves the camera");
				b.add(kR, "Spin attack", "Spin attack, or Yoshi's tongue");
			}
			else
			{
				b.add(kB | kX | kY, "Shoot / run", "Shoot, run, hold on, or Yoshi's tongue");
				b.add(kL, "Crouch / egg", "Crouch, or lay an egg as Yoshi; press in the air to butt-stomp");
				b.add(kR | kGcZ, "Fast-forward");
			}
			b.add(kPlus, "Pause");
			return true;
	}
	return false;
}

bool Metroid(GameInputFn, WiiScheme s, Builder& b)
{
	switch(s)
	{
		case WiiScheme::WII:
			b.add(kA, "Shoot");
			b.add(kB, "Jump");
			b.add(kDown, "Missile");
			b.add(kC, "Morph Ball", "Toggles the Morph Ball");
			b.add(kMinus, "Start");
			b.add(kPlus, "Super missiles", "Toggles super missiles");
			b.add(k1, "Map");
			b.add(k2, "Hint");
			b.add(0, "Point up / down: aim", "Point the Wii Remote up or down to aim");
			b.add(0, "Flick up: spring", "Flick the Wii Remote up while in Morph Ball to spring-jump");
			return true;
		case WiiScheme::CLASSIC:
			b.add(kA, "Shoot");
			b.add(kB, "Jump");
			b.add(kL, "Aim", "Presses the game's L button, to aim");
			b.add(kR, "R button", "Presses the game's R button");
			b.add(kX | kY, "Missile");
			b.add(kZL, "Morph Ball", "Toggles the Morph Ball");
			b.add(kPlus, "Start");
			b.add(kMinus, "Select");
			return true;
		case WiiScheme::GAMECUBE:
			// the Morph Ball is on ZL, which this pad has no button for
			b.add(kA, "Shoot");
			b.add(kB, "Jump");
			b.add(kL, "Aim", "Presses the game's L button, to aim");
			b.add(kR, "R button", "Presses the game's R button");
			b.add(kX | kY, "Missile");
			b.add(kPlus, "Start");
			return true;
	}
	return false;
}

bool Tmnt(GameInputFn fn, WiiScheme s, Builder& b)
{
	const bool arcade = fn == TMNTInput, first = fn == TMNT1Input, third = fn == TMNT3Input;
	const bool gba1 = fn == TMNTGBAInput, gba2 = fn == TMNTGBA2Input;
	// what the shoulder buttons do: roll in the Game Boy games (not at all in the first), a special move in the GBA games
	const char * shoulder = (gba1 || gba2) ? "Special move" : "Roll";

	switch(s)
	{
		case WiiScheme::WII:
			b.add(kA, "Jump");
			b.add(kB, "Swap / charge", "Swaps turtle, or charges an attack (hold up + B for the family super move)");
			b.add(kC, "Roll");
			b.add(kZ, "Special move");
			b.add(0, "Shake: attack", "Shake to attack, pick up a weapon, or throw one away mid-air");
			b.add(0, "Shake Nunchuk: kick", "Shake the Nunchuk for a spin kick");
			return true;
		case WiiScheme::CLASSIC:
			b.add(kB, "Jump");
			b.add(kA, "Attack");
			b.add(kX, "Spin kick");
			if(!first)
				b.add(gba2 ? (kL | kR | kZR) : (kL | kR | kZL | kZR), shoulder);
			if(gba2)
				b.add(kZL, "Look");
			if(arcade)
				b.add(kY, "Swap / charge", "Tap to swap turtle; hold for the family super move");
			else if(third)
				b.add(kY, "Swap turtle");
			b.add(kPlus, "Pause");
			b.add(kMinus, "Select");
			return true;
		case WiiScheme::GAMECUBE:
			b.add(kA, "Jump");
			b.add(kX, "Attack");
			b.add(kY, "Spin kick");
			if(!first)
				b.add(kL | kR, shoulder);
			if(arcade)
				b.add(kB, "Swap / charge", "Tap to swap turtle; hold for the family super move");
			else if(third)
				b.add(kB, "Swap turtle");
			else if(gba2)
				b.add(kB, "Look");
			b.add(kPlus, "Pause");
			b.add(kGcZ, "Select");
			return true;
	}
	return false;
}

bool MortalKombat(GameInputFn, WiiScheme s, Builder& b)
{
	switch(s)
	{
		case WiiScheme::WII:
			b.add(kLeft, "Low punch");
			b.add(kUp, "High punch");
			b.add(kDown, "Low kick");
			b.add(kRight, "High kick");
			b.add(kZ, "Block");
			b.add(kA, "Throw");
			b.add(kC, "Style / run", "Changes style, runs, or changes costume / character");
			b.add(kPlus, "Pause");
			b.add(kMinus, "Costume / character");
			b.add(kStick, "Move / jump", "The Nunchuk stick moves and jumps");
			return true;
		case WiiScheme::CLASSIC:
			b.add(kY, "Low punch");
			b.add(kX, "High punch");
			b.add(kB, "Low kick");
			b.add(kA, "High kick");
			b.add(kR | kZR, "Block");
			b.add(kZR, "Throw");
			b.add(kL | kZL, "Style / run", "Changes style, runs, or changes costume / character");
			b.add(kPlus, "Pause");
			b.add(kMinus, "Costume / character");
			return true;
		case WiiScheme::GAMECUBE:
			b.add(kY, "Low punch");
			b.add(kX, "High punch");
			b.add(kB, "Low kick");
			b.add(kA, "High kick");
			b.add(kR | kGcZ, "Block");
			b.add(kGcZ, "Throw");
			b.add(kL, "Style / run", "Changes style, runs, or changes costume / character");
			b.add(kPlus, "Pause");
			return true;
	}
	return false;
}

bool LegoStarWars(GameInputFn fn, WiiScheme s, Builder& b)
{
	const bool one = fn == LegoStarWars1Input;
	switch(s)
	{
		case WiiScheme::WII:
			b.add(kA, "Jump");
			b.add(kB, "Shoot");
			b.add(kZ, "Force / build", "Use the Force, or build Lego");
			b.add(kC, "Character / talk", "Changes character, or talks");
			b.add(kMinus, "Force power");
			b.add(kPlus, "Start");
			b.add(k1 | k2, "Fast-forward");
			b.add(0, "Swing: lightsaber", "Swing to swing your lightsaber");
			b.add(0, "Flick up: grapple", "Flick the Wii Remote up to grapple");
			return true;
		case WiiScheme::CLASSIC:
		case WiiScheme::GAMECUBE:
			b.add(kA, "Jump");
			b.add(kB | kX | kY, "Shoot / saber");
			b.add(kL | kZL, one ? "Build / Force" : "Character / talk");
			b.add(kR | kZR, one ? "Fast-forward" : "Force / grapple", one ? "Speed, and grapple" : nullptr);
			b.add(kPlus, "Start");
			if(s == WiiScheme::CLASSIC)
				b.add(kMinus, "Select");
			return true;
	}
	return false;
}

bool HarryPotter(GameInputFn fn, WiiScheme s, Builder& b)
{
	const bool gbc = fn == HarryPotter1GBCInput || fn == HarryPotter2GBCInput;
	// the GameCube pad also presses the game's L, R and Select directly in the first three and the Game Boy Color games
	const bool gcDirect = gbc || fn == HarryPotter1Input || fn == HarryPotter2Input || fn == HarryPotter3Input;

	switch(s)
	{
		case WiiScheme::WII:
			b.add(kA, "Interact / Jinx");
			b.add(kB, "Wand / charm", "Wand, charm, or cancel");
			b.add(kZ, "Run / sneak");
			b.add(kC, "Location / jump", "Shows the location name, plays the flute, or jumps (varies by game)");
			b.add(kMinus, "Map / tasks");
			b.add(kPlus, "Pause / menu");
			b.add(k1 | k2, "Change spells");
			b.add(kDpad, "D-pad: subscreen", "Changes subscreen and navigates menus");
			b.add(kStick, "Walk");
			b.add(0, "Wave: cast spell", "Wave to cast a spell (spell gestures are not implemented yet)");
			if(fn == HarryPotter5Input)
				b.add(0, "Point: aim spells", "Order of the Phoenix needs the IR pointer to aim spells");
			return true;
		case WiiScheme::CLASSIC:
		case WiiScheme::GAMECUBE:
			b.add(kA, "Interact / Jinx");
			b.add(kB, "Wand / charm", "Wand, charm, or cancel");
			b.add(kPlus, "Pause / menu");
			if(s == WiiScheme::CLASSIC)
				b.add(kMinus, "Map / tasks");
			if(gbc)
				b.add(kDpad, "D-pad: move");
			else
			{
				b.add(kLeft | kRight, "Subscreen", "Left and right change subscreen");
				b.add(kUp | kDown, "Move");
			}
			if(s == WiiScheme::GAMECUBE && gcDirect)
			{
				b.add(kL, "L button", "Presses the game's L button");
				b.add(kR, "R button", "Presses the game's R button");
				b.add(kGcZ, "Map / tasks", "Presses the game's Select button");
			}
			return true;
	}
	return false;
}

bool MedalOfHonor(GameInputFn fn, WiiScheme s, Builder& b)
{
	switch(s)
	{
		case WiiScheme::WII:
			b.add(kB, "Shoot");
			b.add(kMinus, "Use");
			b.add(kPlus, "Pause / menu", "Pause, objectives, or the menu");
			b.add(k2 | kUp, "Reload");
			b.add(kLeft | kRight, "Change weapon");
			b.add(kDown, "Crouch");
			b.add(kC, "Strafe");
			b.add(k1, "Run");
			b.add(kStick, "Move");
			b.add(0, "Swing up: reload", "Swing the Wii Remote up to reload");
			if(fn == MohUndergroundInput)
				b.add(0, "Point: aim / turn", "Aim and turn with the IR pointer, like an FPS");
			return true;
		case WiiScheme::CLASSIC:
			b.add(kDpad, "D-pad: move");
			b.add(kA, "A button");
			b.add(kB, "B button");
			b.add(kL | kZL, "L button");
			b.add(kR | kZR, "R button");
			b.add(kX | kY, "Fast-forward");
			b.add(kPlus, "Start");
			b.add(kMinus, "Select");
			return true;
		case WiiScheme::GAMECUBE:
			b.add(0, "Not used", "This game does not read the GameCube pad's buttons: only the stick moves");
			return true;
	}
	return false;
}

bool OnePiece(GameInputFn, WiiScheme s, Builder& b)
{
	switch(s)
	{
		case WiiScheme::WII:
			b.add(kA, "Attack");
			b.add(kB, "Jump");
			b.add(kMinus, "Change character");
			b.add(kPlus, "Pause");
			b.add(kC, "Dash", "Double-click and hold to dash");
			b.add(kZ, "Grab");
			b.add(k2, "Fast-forward");
			return true;
		case WiiScheme::GAMECUBE:
			b.add(kA, "Attack");
			b.add(kX, "Attack up");
			b.add(kY, "Jump");
			b.add(kB, "Grab");
			b.add(kR, "Change character");
			b.add(kPlus, "Pause");
			b.add(kL, "Dash", "Double-click and hold to dash");
			b.add(kGcZ, "Grab");
			b.add(kRStick, "Fast-forward");
			return true;
		case WiiScheme::CLASSIC:
			// the standard Classic Controller layout: what each button presses is what the Wii Remote's buttons do
			b.add(kDpad, "D-pad: move");
			b.add(kA, "Jump");
			b.add(kB, "Attack");
			b.add(kL | kZL, "Change character");
			b.add(kR | kZR, "Grab");
			b.add(kX | kY, "Fast-forward");
			b.add(kPlus, "Pause");
			b.add(kMinus, "View");
			return true;
	}
	return false;
}

bool Boktai(GameInputFn, WiiScheme s, Builder& b)
{
	switch(s)
	{
		case WiiScheme::WII:
			// the same with or without a Nunchuk
			b.add(kDpad | kStick, "D-pad / stick: walk");
			b.add(kB, "Gun del Sol", "Fires the Gun del Sol");
			b.add(kA, "Read / open / talk", "Reads signs, opens chests, and talks");
			b.add(kC | k2, "Look / R", "Looks around, or changes subscreen (R)");
			b.add(kZ | k1, "Element / L", "Changes element or subscreen (L); with a Nunchuk, 1 fast-forwards");
			b.add(kPlus, "Start");
			b.add(kMinus, "Select");
			b.add(kHome, "Set weather", "Press Home during play to set the in-game weather; it must be set again every session");
			b.add(0, "Swing: sword", "Swing to swing your sword or weapon");
			b.add(0, "Point at sky: charge", "Point the Wii Remote at the sky to quickly charge your Gun del Sol");
			b.add(0, "Point at ground: shade", "Point it at the ground to block sunlight and stop the gun overheating, or hold it normally for in between");
			return true;
		case WiiScheme::CLASSIC:
			b.add(kDpad | kStick, "D-pad / stick: walk");
			b.add(kB, "Gun del Sol", "Fires the Gun del Sol");
			b.add(kA, "Read / open / talk", "Reads signs, opens chests, and talks");
			b.add(kL | kZL, "Element / L", "Changes element or subscreen (L)");
			b.add(kR | kZR, "Look / R", "Looks around, or changes subscreen (R)");
			b.add(kX | kY, "Fast-forward");
			b.add(kPlus, "Start");
			b.add(kMinus, "Select");
			return true;
		case WiiScheme::GAMECUBE:
			b.add(kDpad | kStick, "D-pad / stick: walk");
			b.add(kB, "Gun del Sol", "Fires the Gun del Sol");
			b.add(kA, "Read / open / talk", "Reads signs, opens chests, and talks");
			b.add(kL, "Element / L", "Changes element or subscreen (L)");
			b.add(kR, "Look / R", "Looks around, or changes subscreen (R)");
			b.add(kPlus, "Start");
			b.add(kGcZ, "Select");
			return true;
	}
	return false;
}

bool Twisted(GameInputFn, WiiScheme s, Builder& b)
{
	switch(s)
	{
		case WiiScheme::WII:
			b.add(kA, "Select");
			b.add(kB, "Cancel");
			b.add(kPlus, "Start");
			b.add(kZ, "Lock item", "Hold Z to lock the current menu item");
			b.add(0, "Rotate: rotate", "Rotate the Wii Remote to rotate");
			return true;
		case WiiScheme::CLASSIC:
			b.add(kDpad, "D-pad: move");
			b.add(kA, "A button");
			b.add(kB, "B button");
			b.add(kL | kZL, "L button");
			b.add(kR | kZR, "R button");
			b.add(kX | kY, "Fast-forward");
			b.add(kPlus, "Start");
			b.add(kMinus, "Select");
			return true;
		case WiiScheme::GAMECUBE:
			b.add(0, "Not used", "This game does not read the GameCube pad's buttons (it plays by tilting a Wii Remote)");
			return true;
	}
	return false;
}

bool Kirby(GameInputFn, WiiScheme s, Builder& b)
{
	switch(s)
	{
		case WiiScheme::WII:
			b.add(kA, "Launch", "Shoots you out of holes in the ground or off clouds");
			b.add(0, "Tilt: tilt the world", "Tilt the Wii Remote to tilt the world");
			b.add(0, "Shake: flick", "Shake to flick Kirby and enemies into the air");
			return true;
		case WiiScheme::CLASSIC:
			b.add(kDpad, "D-pad: move");
			b.add(kA, "Launch", "Shoots you out of holes in the ground or off clouds");
			b.add(kB, "B button");
			b.add(kL | kZL, "L button");
			b.add(kR | kZR, "R button");
			b.add(kX | kY, "Fast-forward");
			b.add(kPlus, "Start");
			b.add(kMinus, "Select");
			return true;
		case WiiScheme::GAMECUBE:
			b.add(0, "Not used", "This game does not read the GameCube pad's buttons (it plays by tilting a Wii Remote)");
			return true;
	}
	return false;
}

bool KidDracula(GameInputFn, WiiScheme s, Builder& b)
{
	const char * fire = "The fire button always shoots a small fireball; hold it to charge and use your selected item instead. Going back to your previous item is automatic unless you have switched again since";
	switch(s)
	{
		case WiiScheme::WII:
			b.add(kA, "Jump");
			b.add(kB, "Use item", "Uses the selected item (hold to charge)");
			b.add(kZ, "NOR weapon", "The fireball");
			b.add(kC, "BAT weapon", "Turns you into a bat for 5 seconds");
			b.add(kPlus, "Pause");
			b.add(kMinus, "Switch item");
			b.add(k1 | k2, "Fast-forward");
			b.add(0, "Fire: fireball", fire);
			return true;
		case WiiScheme::CLASSIC:
			b.add(kA | kB, "Jump");
			b.add(kY, "Use item", "Uses the selected item (hold to charge)");
			b.add(kX, "NOR weapon", "The fireball");
			b.add(kR, "BAT weapon", "Turns you into a bat for 5 seconds");
			b.add(kL, "Fast-forward");
			b.add(kPlus, "Pause");
			b.add(kMinus, "Switch item");
			b.add(0, "Fire: fireball", fire);
			return true;
		case WiiScheme::GAMECUBE:
			b.add(kA, "Jump");
			b.add(kB, "Use item", "Uses the selected item (hold to charge)");
			b.add(kPlus, "Pause");
			b.add(kGcZ, "Switch item");
			b.add(0, "Fire: fireball", fire);
			return true;
	}
	return false;
}

// Mario Kart reads the unified buttons, so it plays the same on every kind of hardware
bool MarioKart(GameInputFn, WiiScheme s, Builder& b)
{
	b.add(kL | kZL, "Use item");
	b.add(kA | kY, "Accelerate");
	b.add(kB | kX, "Brake");
	b.add(kR | kZR, "Jump / slide", "Jump, and power slide");
	b.add(kPlus, "Pause");
	b.add(kMinus, "Select");
	if(s == WiiScheme::WII)
	{
		b.add(0, "Tilt: steer", "Tilt the Wii Remote left and right to steer");
		b.add(0, "Shake: jump", "Shake the Wii Remote up and down to jump");
	}
	return true;
}

// The Star Wars games read the unified buttons too. What A, B and the triggers do differs from game to game; where the
// handler does not say what the Game Boy button is for, the line names the button.
bool StarWars(GameInputFn fn, WiiScheme s, Builder& b)
{
	const char * button = "Presses the game's button of that name";
	if(fn == SWNDAInput)
	{
		b.add(kA, "Jump attack", "The only kind of jump in this game");
		b.add(kB | kX | kY, "Saber", "Lights and swings your lightsaber");
		b.add(kL | kZL, "Block");
		b.add(kR | kZR, "Force");
		b.add(kDpad, "D-pad: Force power", "Changes your Force power");
		b.add(k1 | k2, "Fast-forward");
	}
	else if(fn == SWObiWanInput)
	{
		b.add(kA, "Jump");
		b.add(kB | kX | kY, "Saber / shoot", "Saber or blaster");
		b.add(kL | kZL, "Weapon 3", "Switches to the game's third weapon and uses it");
		b.add(kR | kZR, "Fast-forward");
	}
	else if(fn == SWYodaStoriesInput)
	{
		b.add(kA, "Drag object");
		b.add(kB | kX | kY, "Saber / shoot");
		b.add(kL | kZL, "L button", button);
		b.add(kR | kZR, "Fast-forward");
	}
	else if(fn == SWEpisode2Input || fn == SWJediPowerBattlesInput)
	{
		b.add(kA, "Jump");
		b.add(kB | kX | kY, "Saber / shoot");
		b.add(kL | kZL, "L button", button);
		b.add(kR | kZR, "R button", button);
	}
	else
	{
		// Episodes 3 to 6 and the Trilogy: A and B press the game's A and B; in Episodes 4 to 6, R fast-forwards
		const bool fast = fn == SWEpisode4Input || fn == SWEpisode5Input || fn == SWEpisode6Input;
		b.add(kA, "A button", button);
		b.add(kB | kX | kY, "B button", button);
		b.add(kL | kZL, "L button", button);
		b.add(kR | kZR, fast ? "Fast-forward" : "R button", fast ? nullptr : button);
	}
	b.add(kPlus, "Start");
	b.add(kMinus, "Select");
	if(s == WiiScheme::WII)
		b.add(0, "Shake: saber", "Shake the Wii Remote to swing your lightsaber");
	return true;
}

// The Lord of the Rings and Castlevania games read each kind of hardware's own buttons, not the unified ones, and have
// a handful of Game Boy Advance buttons to give out: those lines name the button. The Nunchuk (and, in Castlevania, the
// Classic Controller) is written out by what each control does. The Wii U Pro Controller and the GamePad play as a
// Classic Controller.
bool Isolated(GameInputFn fn, int hw, Builder& b)
{
	const bool gba = cartridgeType == CARTRIDGE_GBA;
	const bool hobbit = fn == HobbitInput, fellowship = fn == FellowshipOfTheRingInput, king = fn == ReturnOfTheKingInput;
	const bool adventure = fn == CastlevaniaAdventureInput, belmont = fn == CastlevaniaBelmontInput;
	const bool legends = fn == CastlevaniaLegendsInput, circle = fn == CastlevaniaCircleMoonInput;
	const bool castlevania = adventure || belmont || legends || circle;

	switch(hw)
	{
		case INPUT_HW_GAMECUBE:
			b.add(kA, "A button");
			b.add(kB, "B button");
			b.add(kPlus, "Start");
			b.add(kGcZ, "Select");
			if(gba)
			{
				b.add(kL, "L button");
				b.add(kR, "R button");
			}
			return true;

		case INPUT_HW_WIIMOTE:
			// the Wii Remote on its own is held sideways
			b.add(kUp, "Move left", "The Wii Remote is held sideways, so the D-pad is turned");
			b.add(kDown, "Move right", "The Wii Remote is held sideways, so the D-pad is turned");
			b.add(kLeft, "Move down", "The Wii Remote is held sideways, so the D-pad is turned");
			b.add(kRight, "Move up", "The Wii Remote is held sideways, so the D-pad is turned");
			b.add(k2, "A button");
			b.add(k1, "B button");
			b.add(kA, gba ? "R button" : "Fast-forward");
			b.add(kB, gba ? "L button" : "Fast-forward");
			b.add(kPlus, "Start");
			b.add(kMinus, "Select");
			return true;

		case INPUT_HW_NUNCHUK:
			b.add(kDpad, "D-pad: move");
			b.add(kStick, "Move");
			if(hobbit)
			{
				b.add(kB, "Ability");
				b.add(kA, "Use");
				b.add(kPlus, "Pause");
				b.add(kMinus, "Items");
				b.add(kC, "Speed", "Fast-forward");
				b.add(kZ, "Change skill");
				b.add(0, "Shake: attack", "Shake the Wii Remote to attack");
			}
			else if(fellowship)
			{
				b.add(kB, "Cancel");
				b.add(kA, "Use");
				b.add(kPlus, "Pause");
				b.add(kMinus, "Items");
				b.add(kC, "Speed", "Fast-forward");
				b.add(kZ, "Change character");
				b.add(k1, "Select");
				b.add(0, "Shake: cancel", "Shake the Wii Remote to cancel");
			}
			else if(king)
			{
				b.add(kB, "Ability");
				b.add(kA, "Use");
				b.add(kPlus, "Pause");
				b.add(kMinus, "Items");
				b.add(kC, "Speed", "Fast-forward");
				b.add(kZ, "Change skill");
				b.add(0, "Shake: attack", "Shake the Wii Remote to attack");
			}
			else if(castlevania)
			{
				b.add(kB, "Attack");
				b.add(kC, "Jump");
				b.add(kZ, "Guard", "Crouch and guard");
				b.add(kPlus, "Pause");
				b.add(kMinus, "Select");
				if(!adventure)
					b.add(kA, "Sub-weapon", "Throws your sub-weapon");
				b.add(kA | kB, "A + B: speed", "Press A and B together to fast-forward");
				if(legends || circle)
					b.add(kDown, "Hyper", "Hyper attack");
				if(circle)
				{
					b.add(k1, "L button");
					b.add(k2, "R button");
				}
				b.add(0, "Shake: attack", "Shake the Wii Remote to attack");
			}
			return true;

		case INPUT_HW_CLASSIC:
		case INPUT_HW_WUPC:
		case INPUT_HW_DRC:
			if(castlevania)
			{
				b.add(kB, "Attack");
				b.add(kR, "Jump");
				b.add(kL, "Guard", "Crouch and guard");
				b.add(kPlus, "Pause");
				b.add(kMinus, "Select");
				b.add(kA, "Speed", "Fast-forward");
				if(!adventure)
					b.add(kY, "Sub-weapon", "Throws your sub-weapon");
				if(legends || circle)
					b.add(kX, "Hyper", "Hyper attack");
				if(circle)
					b.add(kL | kR, "L / R button", "L and R also press the game's L and R buttons");
			}
			else
			{
				b.add(kDpad, "D-pad: move");
				b.add(kA, "A button");
				b.add(kB, "B button");
				b.add(kL | kZL, "L button");
				b.add(kR | kZR, "R button");
				b.add(kX | kY, "Fast-forward");
				b.add(kPlus, "Start");
				b.add(kMinus, "Select");
			}
			return true;
	}
	return false;
}

} // namespace

WiiScheme WiiSchemeFor(int hw)
{
	switch(hw)
	{
		case INPUT_HW_GAMECUBE: return WiiScheme::GAMECUBE;
		case INPUT_HW_WIIMOTE:
		case INPUT_HW_NUNCHUK: return WiiScheme::WII;
		default: return WiiScheme::CLASSIC;
	}
}

bool WiiControlsFor(int hw, WiiControls& out)
{
	out.actions.clear();
	const WiiScheme scheme = WiiSchemeFor(hw);
	const GameInputFn fn = CurrentGameInput();
	Builder b(out);

	bool found = false;
	switch(FamilyOf(fn))
	{
		case Family::Zelda: found = Zelda(fn, scheme, b); break;
		case Family::Mario: found = Mario(fn, scheme, b); break;
		case Family::Metroid: found = Metroid(fn, scheme, b); break;
		case Family::Turtles: found = Tmnt(fn, scheme, b); break;
		case Family::MortalKombat: found = MortalKombat(fn, scheme, b); break;
		case Family::LegoStarWars: found = LegoStarWars(fn, scheme, b); break;
		case Family::HarryPotter: found = HarryPotter(fn, scheme, b); break;
		case Family::MedalOfHonor: found = MedalOfHonor(fn, scheme, b); break;
		case Family::OnePiece: found = OnePiece(fn, scheme, b); break;
		case Family::Boktai: found = Boktai(fn, scheme, b); break;
		case Family::Twisted: found = Twisted(fn, scheme, b); break;
		case Family::Kirby: found = Kirby(fn, scheme, b); break;
		case Family::KidDracula: found = KidDracula(fn, scheme, b); break;
		case Family::MarioKart: found = MarioKart(fn, scheme, b); break;
		case Family::StarWars: found = StarWars(fn, scheme, b); break;
		case Family::Isolated: found = Isolated(fn, hw, b); break;
		case Family::None: break;
	}
	if(!found)
		out.actions.clear();
	return found;
}

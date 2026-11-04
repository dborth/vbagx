/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * wiicontrols.cpp
 *
 * See wiicontrols.h. The wording comes from the README's "Special Wii Controls"
 * section. Games the README has no controls written down for (Mario Kart, the
 * non-Lego Star Wars games, The Lord of the Rings, Castlevania) have no table
 * here, so the screen shows nothing for them.
 ***************************************************************************/

#include "wiicontrols.h"
#include "gameinput.h"
#include "drivers/InputData.h"

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
	OnePiece, Boktai, Twisted, Kirby, KidDracula
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
			break;
	}
	return false;
}

bool Metroid(GameInputFn, WiiScheme s, Builder& b)
{
	if(s != WiiScheme::WII)
		return false;
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
}

bool Tmnt(GameInputFn, WiiScheme s, Builder& b)
{
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
			b.add(0, "PlayStation layout", "The Classic Controller uses the PlayStation control scheme instead");
			return true;
		case WiiScheme::GAMECUBE:
			break;
	}
	return false;
}

bool MortalKombat(GameInputFn, WiiScheme s, Builder& b)
{
	if(s != WiiScheme::WII)
		return false;
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
}

bool LegoStarWars(GameInputFn, WiiScheme s, Builder& b)
{
	if(s != WiiScheme::WII)
		return false;
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
}

bool HarryPotter(GameInputFn fn, WiiScheme s, Builder& b)
{
	if(s != WiiScheme::WII)
		return false;
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
}

bool MedalOfHonor(GameInputFn fn, WiiScheme s, Builder& b)
{
	if(s != WiiScheme::WII)
		return false;
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
			break;
	}
	return false;
}

bool Boktai(GameInputFn, WiiScheme s, Builder& b)
{
	// the same with or without a Nunchuk
	if(s != WiiScheme::WII)
		return false;
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
}

bool Twisted(GameInputFn, WiiScheme s, Builder& b)
{
	if(s != WiiScheme::WII)
		return false;
	b.add(kA, "Select");
	b.add(kB, "Cancel");
	b.add(kPlus, "Start");
	b.add(kZ, "Lock item", "Hold Z to lock the current menu item");
	b.add(0, "Rotate: rotate", "Rotate the Wii Remote to rotate");
	return true;
}

bool Kirby(GameInputFn, WiiScheme s, Builder& b)
{
	if(s != WiiScheme::WII)
		return false;
	b.add(kA, "Launch", "Shoots you out of holes in the ground or off clouds");
	b.add(0, "Tilt: tilt the world", "Tilt the Wii Remote to tilt the world");
	b.add(0, "Shake: flick", "Shake to flick Kirby and enemies into the air");
	return true;
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
			b.add(kR | kZR, "BAT weapon", "Turns you into a bat for 5 seconds");
			b.add(kPlus, "Pause");
			b.add(kMinus, "Switch item");
			b.add(0, "Fire: fireball", fire);
			return true;
		case WiiScheme::GAMECUBE:
			break;
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

bool WiiControlsFor(WiiScheme scheme, WiiControls& out)
{
	out.actions.clear();
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
		case Family::None: break;
	}
	if(!found)
		out.actions.clear();
	return found;
}

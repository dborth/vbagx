/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * mappingui.cpp
 *
 * See mappingui.h.
 ***************************************************************************/

#include <string>
#include <vector>
#include <algorithm>
#include <string.h>
#include <math.h>

#include "libgui/Gui.h"
#include "drivers/InputController.h"
#include "mappingui.h"
#include "mappingmodel.h"
#include "wiicontrols.h"
#include "mappingchooser.h"
#include "controllerorder.h"
#include "controllerview.h"
#include "menupopover.h"
#include "menustyle.h"
#include "menuart.h"
#include "menu.h"
#include "input.h"
#include "button_mapping.h"
#include "vbagx.h"

namespace {

// The hardware being mapped (-1 until chosen: the screen starts on the controller in use)
int mapHardware = -1;

const int kSideW = 172;			// a column of rows beside the drawing
const int kRowGap = 1;
const int kResetH = 22;			// the Reset button's height
const int kWiiBarW = 188;			// the Match Wii Controls switch, in the strip above the drawing
const int kWiiBarH = 32;
const int kWiiBarPadX = 12;
const int kWiiLabelW = kWiiBarW - 2 * kWiiBarPadX - 8 - MenuSwitch::W;
const int kWiiDimAlpha = 110;		// the rows and drawing while the mappings are not in use
const int kWiiRowH = 29;			// about as tall as a row, to fit the lines of a game's controls to a column
const int kWiiHeadH = 19;			// and a heading
const int kCardPad = 8;
const int kBodyW = 640 - 2 * MENU_PAGE_MARGIN;
const int kContentW = kBodyW - 2 * kCardPad;

const InputPadData * PadData()
{
	return controller[0] ? &controller[0]->getPadData() : nullptr;
}

bool Contains(const std::vector<int>& v, int x)
{
	return std::find(v.begin(), v.end(), x) != v.end();
}

// The hardware that can be mapped: as the old hardware screen offered it (the GamePad only while it is connected)
std::vector<int> HardwareList()
{
	std::vector<int> v;
#ifndef __WIIU__
	v.push_back(INPUT_HW_GAMECUBE);
#endif
#ifndef HW_DOL
	v.push_back(INPUT_HW_WIIMOTE);
	v.push_back(INPUT_HW_NUNCHUK);
	v.push_back(INPUT_HW_CLASSIC);
	v.push_back(INPUT_HW_WUPC);
	const InputPadData * pad = PadData();
	if(pad && pad->hw_connected[INPUT_HW_DRC])
		v.push_back(INPUT_HW_DRC);
#endif
	return v;
}

// The hardware that opened the screen (the controller whose A pressed Button Mappings), or -1 when not known
int openedHardware = -1;

// The controller being used on a channel: the one that is pressing buttons. A Wiimote's own buttons are shared by what is
// plugged into it, so with a Nunchuk, Classic Controller or Pro Controller on it that is the one (see controllerorder.h).
// -1 when nothing on the channel is pressing anything.
int HardwareInUse(int channel)
{
	if(channel < 0 || channel >= 4 || !controller[channel])
		channel = 0;
	if(!controller[channel])
		return -1;
	const InputPadData& pad = controller[channel]->getPadData();
	static const int pressing[] = { INPUT_HW_DRC, INPUT_HW_WUPC, INPUT_HW_CLASSIC, INPUT_HW_GAMECUBE, INPUT_HW_WIIMOTE };
	for(size_t i = 0; i < sizeof(pressing) / sizeof(pressing[0]); i++)
	{
		const int hw = pressing[i];
		if(!pad.hw_connected[hw] || (pad.hw_buttons_d[hw] | pad.hw_buttons_h[hw] | pad.hw_buttons_r[hw]) == 0)
			continue;
		if(hw == INPUT_HW_WIIMOTE)
		{
			const int console = ControllerOrder::ConsoleHardware(pad);
			return console == INPUT_HW_MAX ? hw : console;
		}
		return hw;
	}
	return -1;
}

// The hardware to start on: the controller that opened the screen; failing that, the controller in use on channel 0,
// chosen the way the controller strip chooses its icon (see controllerorder.h)
int DefaultHardware(const std::vector<int>& list)
{
	if(openedHardware >= 0 && Contains(list, openedHardware))
		return openedHardware;
	const InputPadData * pad = PadData();
	if(pad)
	{
		const int hw = ControllerOrder::InUse(*pad);
		if(Contains(list, hw))
			return hw;
	}
	return list[0];
}

void ResolveHardware()
{
	const std::vector<int> list = HardwareList();
	if(mapHardware < 0 || !Contains(list, mapHardware))
		mapHardware = DefaultHardware(list);
}

// The name of the in-game menu option that bypasses these mappings
const char * WiiControlsOptionName()
{
#ifndef HW_DOL
	return "Match Wii Controls";
#else
	return "Match GC Controls";
#endif
}

GLYPH GlyphFor(MAP_ICON icon, const char *& letter)
{
	letter = "";
	switch(icon)
	{
		case MAP_ICON::A: letter = "A"; return GLYPH::BUTTON;
		case MAP_ICON::B: letter = "B"; return GLYPH::BUTTON;
		case MAP_ICON::X: letter = "X"; return GLYPH::BUTTON;
		case MAP_ICON::Y: letter = "Y"; return GLYPH::BUTTON;
		case MAP_ICON::L: letter = "L"; return GLYPH::BUMPER;
		case MAP_ICON::R: letter = "R"; return GLYPH::BUMPER;
		case MAP_ICON::START: return GLYPH::PLUS;
		case MAP_ICON::SELECT: return GLYPH::MINUS;
		case MAP_ICON::UP: return GLYPH::UP;
		case MAP_ICON::DOWN: return GLYPH::DOWN;
		case MAP_ICON::LEFT: return GLYPH::LEFT;
		case MAP_ICON::RIGHT: return GLYPH::RIGHT;
		case MAP_ICON::NONE: break;
	}
	return GLYPH::BUTTON;
}

// The row that has this btnmap index
const MapRow * FindRow(int index)
{
	const MapProfile& p = MappingProfile();
	for(int side = 0; side < 2; side++)
	{
		const std::vector<MapGroup>& groups = side ? p.right : p.left;
		for(size_t g = 0; g < groups.size(); g++)
			for(size_t i = 0; i < groups[g].rows.size(); i++)
				if(groups[g].rows[i].index == index)
					return &groups[g].rows[i];
	}
	return nullptr;
}

// The icon for a control of a game's own controls. The Nunchuk's C and Z are the left trigger bits; the GameCube pad's
// Z is the right Z trigger and its C-stick is the right stick.
struct ControlGlyph {
	GLYPH kind;
	const char * letter;
};

ControlGlyph GlyphForControl(WiiScheme scheme, uint32_t bit)
{
	switch(bit)
	{
		case INPUT_BTN_A: return { GLYPH::BUTTON, "A" };
		case INPUT_BTN_B: return { GLYPH::BUTTON, "B" };
		case INPUT_BTN_X: return { GLYPH::BUTTON, "X" };
		case INPUT_BTN_Y: return { GLYPH::BUTTON, "Y" };
		case INPUT_BTN_1: return { GLYPH::BUTTON, "1" };
		case INPUT_BTN_2: return { GLYPH::BUTTON, "2" };
		case INPUT_BTN_UP: return { GLYPH::UP, "" };
		case INPUT_BTN_DOWN: return { GLYPH::DOWN, "" };
		case INPUT_BTN_LEFT: return { GLYPH::LEFT, "" };
		case INPUT_BTN_RIGHT: return { GLYPH::RIGHT, "" };
		case INPUT_BTN_PLUS: return { GLYPH::PLUS, "" };
		case INPUT_BTN_MINUS: return { GLYPH::MINUS, "" };
		case INPUT_BTN_HOME: return { GLYPH::HOME, "" };
		case INPUT_TRIGGER_L: return scheme == WiiScheme::WII ? ControlGlyph{ GLYPH::BUTTON, "C" } : ControlGlyph{ GLYPH::BUMPER, "L" };
		case INPUT_TRIGGER_R: return { GLYPH::BUMPER, "R" };
		case INPUT_TRIGGER_ZL: return { GLYPH::BUMPER, scheme == WiiScheme::WII ? "Z" : "ZL" };
		case INPUT_TRIGGER_ZR: return { GLYPH::BUMPER, scheme == WiiScheme::GAMECUBE ? "Z" : "ZR" };
		case INPUT_THUMB_L: return { GLYPH::STICK, scheme == WiiScheme::WII ? "" : "L" };
		case INPUT_THUMB_R: return { GLYPH::STICK, scheme == WiiScheme::GAMECUBE ? "C" : "R" };
	}
	return { GLYPH::BUTTON, "" };
}

// One line of a game's controls, placed
struct WiiRowPlan {
	const WiiAction * action;
	std::vector<int> spots;		// the controls on the drawing it is on
	float x, y;					// where, on the drawing
};

bool ByY(const WiiRowPlan& a, const WiiRowPlan& b) { return a.y < b.y; }

int ColumnHeight(const std::vector<WiiRowPlan>& rows, const std::vector<WiiRowPlan>& extra)
{
	return (int)rows.size() * (kWiiRowH + kRowGap) + (extra.empty() ? 0 : kWiiHeadH + (int)extra.size() * (kWiiRowH + kRowGap));
}

// Moves the line nearest the middle of the drawing from one column to the other
void MoveNearestMiddle(std::vector<WiiRowPlan>& from, std::vector<WiiRowPlan>& to, float mid)
{
	size_t best = 0;
	float bestD = 1e9f;
	for(size_t i = 0; i < from.size(); i++)
	{
		const float d = fabsf(from[i].x - mid);
		if(d < bestD)
		{
			bestD = d;
			best = i;
		}
	}
	to.push_back(from[best]);
	from.erase(from.begin() + (long)best);
	std::stable_sort(to.begin(), to.end(), ByY);
}

// Where the lines of a game's controls go: each button's line in the column on its side of the drawing, top to bottom,
// the columns evened out (a Wiimote is a single strip, so it is split down the middle); then the lines about something
// other than a button (a shake, a swing) under a heading in whichever column has more room, as many as fit in `budget`.
// A button the drawing has no control for (the Nunchuk's C and Z on a Wiimote alone) has no line.
void PlanWiiColumns(const ControllerLayout& layout, const WiiControls& controls, int budget,
	std::vector<WiiRowPlan> side[2], std::vector<WiiRowPlan> extra[2])
{
	std::vector<WiiRowPlan> buttons, others;
	for(size_t i = 0; i < controls.actions.size(); i++)
	{
		WiiRowPlan p;
		p.action = &controls.actions[i];
		p.x = p.y = 0.0f;
		if(p.action->ids == 0)
		{
			others.push_back(p);
			continue;
		}
		for(int bit = 0; bit < 32; bit++)
		{
			const uint32_t b = 1u << bit;
			const ControlSpot * spot = (p.action->ids & b) ? layout.find((int)b) : nullptr;
			if(!spot)
				continue;
			p.spots.push_back((int)b);
			p.x += spot->x + spot->w / 2.0f;
			p.y += spot->y + spot->h / 2.0f;
		}
		if(p.spots.empty())
			continue;
		p.x /= (float)p.spots.size();
		p.y /= (float)p.spots.size();
		buttons.push_back(p);
	}
	std::stable_sort(buttons.begin(), buttons.end(), ByY);

	float minX = 1e9f, maxX = -1e9f;
	for(size_t i = 0; i < buttons.size(); i++)
	{
		minX = std::min(minX, buttons[i].x);
		maxX = std::max(maxX, buttons[i].x);
	}
	const bool strip = (maxX - minX) < layout.width * 0.2f;
	const float mid = layout.width / 2.0f;
	for(size_t i = 0; i < buttons.size(); i++)
	{
		const bool toRight = strip ? i >= (buttons.size() + 1) / 2 : buttons[i].x >= mid;
		side[toRight ? 1 : 0].push_back(buttons[i]);
	}
	// even the columns out: the line nearest the middle goes from the longer to the shorter
	while(side[0].size() > side[1].size() + 2 || side[1].size() > side[0].size() + 2)
	{
		const int from = side[0].size() > side[1].size() ? 0 : 1;
		MoveNearestMiddle(side[from], side[1 - from], mid);
	}
	for(int c = 0; c < 2; c++)
	{
		std::stable_sort(side[c].begin(), side[c].end(), ByY);
		while(!side[c].empty() && ColumnHeight(side[c], extra[c]) > budget)	// more than fits: the last ones go
			side[c].pop_back();
	}

	// the lines that are not a button share one heading, so they go in one column: the one that has them, else the
	// shorter. If they do not fit, a button's line moves over to the other column to make room, if that has room.
	for(size_t i = 0; i < others.size(); i++)
	{
		for(int attempt = 0; attempt < 8; attempt++)
		{
			const int to = extra[0].empty() != extra[1].empty() ? (extra[0].empty() ? 1 : 0)
				: (ColumnHeight(side[0], extra[0]) <= ColumnHeight(side[1], extra[1]) ? 0 : 1);
			const int need = kWiiRowH + (extra[to].empty() ? kWiiHeadH : 0);
			if(ColumnHeight(side[to], extra[to]) + need <= budget)
			{
				extra[to].push_back(others[i]);
				break;
			}
			if(side[to].empty() || ColumnHeight(side[1 - to], extra[1 - to]) + kWiiRowH + kRowGap > budget)
				break;
			MoveNearestMiddle(side[to], side[1 - to], mid);
		}
	}
}

// What a row uses by default. Defaults are only set by ResetControls (all of a controller's rows at once, and for the
// Wii U Pro controller and the GamePad together), so this has it set them and puts everything back.
uint32_t DefaultButton(int hw, int index)
{
	static uint32_t saved[INPUT_HW_MAX][MAXJP];
	memcpy(saved, btnmap, sizeof(saved));
	ResetControls(hw);
	const uint32_t value = btnmap[hw][index];
	memcpy(btnmap, saved, sizeof(saved));
	return value;
}

// What a row says it is mapped to
const char * ValueText(int hw, int index)
{
	const char * name = MappingButtonName(hw, btnmap[hw][index]);
	return name ? name : Tr("None");
}

} // namespace

/****************************************************************************
 * The screen
 ***************************************************************************/

namespace {

class MappingView : public GuiOverlay
{
	public:
		enum class Pick { NONE, USING };

		MappingView(int hw, bool selectWii = false);

		GuiTextButton& backButton() { return *backBtn; }
		GuiTextButton& resetButton() { return *resetBtn; }
		//!Whether a row was clicked since the last call; `index` is its btnmap index
		bool takeRow(int& index)
		{
			if(!rowRequested)
				return false;
			rowRequested = false;
			index = requestedRow;
			return true;
		}
		Pick takePick() { const Pick p = pickRequest; pickRequest = Pick::NONE; return p; }
		//!Whether the Match Wii Controls switch was clicked since the last call; `wii` is what it was turned to
		bool takeWiiChoice(bool& wii)
		{
			if(!wiiRequested)
				return false;
			wiiRequested = false;
			wii = wiiChoice;
			return true;
		}
		GuiPopover::Anchor anchorForChip() { return GuiPopover::Anchor(*usingChip); }
		GuiPopover::Anchor anchorForRow(int index);
		void selectRow(int index);
		void selectChip() { usingChip->select(); }

	private:
		GuiCard& bindingRow(const MapRow& r, bool onLeft, int width);
		GuiCard& wiiToggle(bool on, bool animate);
		GuiCard& wiiRow(const WiiRowPlan& plan, WiiScheme scheme, bool onLeft, int width);

		UiScope ui;
		int hardware;
		ControllerView * view;
		std::string status;
		std::vector<std::pair<int, GuiCard*>> rows;		// btnmap index, row
		std::vector<bool> rowOnLeft;
		std::vector<std::vector<int>> wiiSpots;		// when the rows are a game's controls: the controls each is on, by row index
		GuiTextButton * backBtn;
		GuiTextButton * resetBtn;
		GuiCard * usingChip;
		GuiCard * wiiCard;		// the Match Wii Controls switch, when the game has them
		bool wiiRequested;
		bool wiiMappingsApply;	// the game's own controls leave the mappings in charge on this controller
		bool wiiChoice;
		bool rowRequested;
		int requestedRow;
		Pick pickRequest;
};

GuiCard& MappingView::bindingRow(const MapRow& r, bool onLeft, int width)
{
	const int padX = 8;
	const int hw = hardware;
	const int index = r.index;
	const char * name = r.name;

	GuiLabel& value = ui.label().fontSize(UI_FONT_SMALL + 1).nudgeY(2).accent().bind([hw, index](UiText& t){ t.set(ValueText(hw, index)); });
	GuiLabel& label = ui.label(Tr(name)).fontSize(UI_FONT_SMALL + 2).nudgeY(2);

	GuiLayout& content = ui.row().size(width - 2 * padX, 0).gap(6).align(LAYOUT_ALIGN::CENTRE);
	if(r.icon != MAP_ICON::NONE)
	{
		const char * letter;
		const GLYPH kind = GlyphFor(r.icon, letter);
		content.add(ui.glyph(kind, letter));
	}
	// long names are cut rather than spilling into the value
	label.setMaxWidth(width - 2 * padX - (r.icon != MAP_ICON::NONE ? 30 : 0) - 60);
	value.setMaxWidth(60);
	label.setScroll(SCROLL::HORIZONTAL);
	content.add(label).addSpace().add(value);

	GuiCard& row = ui.card();
	row.size(width, 0).style(MenuRowHighlightStyle()).pad(padX, 4).corners(GuiCorners(9.0f));
	// while Match Wii Controls is overriding this game, what is set here is kept but not used: the rows are shown faded and
	// can not be selected or clicked (the hint at the top says why)
	if(WiiControlsOverrideActive())
		row.selectable(false);
	else
	{
		row.onFocus([this, name]{ status = TrFormat("A changes what %s does", Tr(name)); });
		row.onClick([this, index]{ requestedRow = index; rowRequested = true; });
	}
	row.add(content);
	rows.push_back(std::make_pair(index, &row));
	rowOnLeft.push_back(onLeft);
	return row;
}

//!One line of a game's own controls: the icon of the control (or of each of the first two it is on), what it does, and
//!the whole story as the hint. It can be selected to show where the control is on the drawing, but not changed.
GuiCard& MappingView::wiiRow(const WiiRowPlan& plan, WiiScheme scheme, bool onLeft, int width)
{
	const int padX = 8;
	const WiiAction& a = *plan.action;
	const int index = (int)wiiSpots.size();
	wiiSpots.push_back(plan.spots);

	GuiLayout& content = ui.row().size(width - 2 * padX, 0).gap(6).align(LAYOUT_ALIGN::CENTRE);

	// an icon for each of the first two controls; a whole D-pad has none (the drawing shows it)
	const uint32_t dpad = INPUT_BTN_UP | INPUT_BTN_DOWN | INPUT_BTN_LEFT | INPUT_BTN_RIGHT;
	uint32_t shown = 0;
	for(size_t i = 0; i < plan.spots.size(); i++)
		shown |= (uint32_t)plan.spots[i];
	if((shown & dpad) == dpad)
		shown &= ~dpad;
	int icons = 0;
	for(int bit = 0; bit < 32 && icons < 2; bit++)
		if(shown & (1u << bit))
		{
			const ControlGlyph g = GlyphForControl(scheme, 1u << bit);
			content.add(ui.glyph(g.kind, g.letter));
			icons++;
		}

	GuiLabel& label = ui.label(Tr(a.name)).fontSize(UI_FONT_SMALL + 2).nudgeY(2);
	label.setMaxWidth(width - 2 * padX - icons * 30);
	label.setScroll(SCROLL::HORIZONTAL);
	content.add(label);

	const std::string hint = Tr(a.detail ? a.detail : a.name);
	GuiCard& row = ui.card();
	row.size(width, 0).style(MenuRowHighlightStyle()).pad(padX, 4).corners(GuiCorners(9.0f));
	row.onFocus([this, hint]{ status = hint; });
	// nothing to change here; a click that did nothing would leave the row stuck in its clicked state
	row.onClick([]{});
	row.add(content);
	rows.push_back(std::make_pair(index, &row));
	rowOnLeft.push_back(onLeft);
	return row;
}

//!The switch that has the game use its own Wii controls instead of these mappings. Clicking it asks the screen to turn it
//!(the screen is rebuilt to show the other rows); `animate` starts the knob where it was, so it slides to its new place.
GuiCard& MappingView::wiiToggle(bool on, bool animate)
{
	GuiLabel& label = ui.label(Tr(WiiControlsOptionName())).fontSize(UI_FONT_SMALL + 1).nudgeY(2);
	label.setMaxWidth(kWiiLabelW);
	label.setScroll(SCROLL::HORIZONTAL);
	MenuSwitch& knob = ui.make<MenuSwitch>(animate ? !on : on);
	if(animate)
		knob.setOn(on);

	GuiLayout& line = ui.row().size(kWiiBarW - 2 * kWiiBarPadX, 0).gap(8).align(LAYOUT_ALIGN::CENTRE)
		.add(label).addSpace().add(knob);

	GuiCard& card = ui.card();
	card.size(kWiiBarW, kWiiBarH).style(MenuChipStyle()).corners(GuiCorners(kWiiBarH / 2.0f)).pad(kWiiBarPadX, 0);
	card.onFocus([this, on]{ status = Tr(on ? "A goes back to using the mappings below" : "A uses this game's own Wii controls instead"); });
	card.onClick([this, on]{
		wiiChoice = !on;
		wiiRequested = true;
	});
	card.add(line);
	wiiCard = &card;
	return card;
}

MappingView::MappingView(int hw, bool selectWii) :
	hardware(hw), view(nullptr), status("Move over a button to see what it uses"),
	backBtn(nullptr), resetBtn(nullptr), usingChip(nullptr), wiiCard(nullptr), wiiRequested(false), wiiChoice(false),
	rowRequested(false), requestedRow(0), pickRequest(Pick::NONE)
{
	const int sw = platform->getVideo()->getScreenWidth();
	const int sh = platform->getVideo()->getScreenHeight();
	const int margin = MENU_PAGE_MARGIN;
	setFixedSize(sw, sh);

	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();

	// ---- header: the page name with the hint under it, and the chip that chooses the hardware
	GuiLabel& title = ui.label(Tr("Button Mappings")).fontSize(UI_FONT_TITLE - 2).textColor(MenuChromeText());
	GuiLabel& hint = ui.label().fontSize(UI_FONT_SMALL + 2).textColor(MenuChromeTextMuted()).bind([this](UiText& t){ t.set(status.c_str()); });
	hint.setMaxWidth(260);
	hint.setScroll(SCROLL::HORIZONTAL);
	GuiLayout& titleBlock = ui.column().gap(4).size(260, 0).add(title).add(hint);

	const int h = hardware;
	GuiCard& usingChipCard = MenuPopover_Chip(ui, 196, "USING", [h](UiText& t){ t.set(Tr(ctrlrName[h])); });
	usingChipCard.onClick([this]{ pickRequest = Pick::USING; });
	usingChipCard.onFocus([this]{ status = "A chooses the controller to map"; });
	usingChip = &usingChipCard;

	GuiLayout& header = ui.row().size(sw, MENU_BAR_TOP_H).pad(margin, 0).gap(14).align(LAYOUT_ALIGN::CENTRE)
		.add(titleBlock).addSpace().add(usingChipCard);

	// ---- body: one card with the rows either side of the drawing
	const MapProfile& profile = MappingProfile();
	const bool sideways = EmuSettings.wiimoteOrientation == WIIMOTE_ORIENTATION_HORIZONTAL;
	const ControllerLayout& layout = MappingLayout(hardware, sideways);
	const int bodyH = sh - MENU_BAR_TOP_H - MENU_BAR_BOTTOM_H - 8;
	const int stageH = bodyH - 2 * kCardPad;
	const int drawW = kContentW - 2 * kSideW - 2 * 6;
	// With the Match Wii Controls switch up, the stage starts below it and the drawing is a little smaller, so the tallest
	// drawing (the Wiimote) clears the banner
	const bool hasWii = GameHasWiiControls();
	const int stageTop = hasWii ? kWiiBarH + 2 : 0;
	const float scale = MappingScale(layout, (float)drawW, (float)(hasWii ? stageH - stageTop - 12 : stageH - 24));
	view = &ui.make<ControllerView>(layout, scale);
	// no text on the drawing: the rows name each control
	view->setLabelFilter([](int){ return false; });
	// lit while held, so pressing a button on the controller shows which control it is
	view->setLit([hw](int id){
		const InputPadData * pad = PadData();
		return pad && id > 0 && (pad->hw_buttons_h[hw] & (uint32_t)id) != 0;
	});

	// While the game's own Wii controls are replacing the mappings, and we have them written down, the rows are what they
	// do (laid out by where each control is on the drawing); otherwise they are the mappings
	WiiControls wiiControls;
	const WiiScheme wiiScheme = WiiSchemeFor(hardware);
	const bool showWii = WiiControlsOverrideActive() && WiiControlsFor(hardware, wiiControls);
	wiiMappingsApply = WiiControlsOverrideActive() && !showWii && wiiControls.mappings;
	if(WiiControlsOverrideActive() && !showWii)
		status = TrFormat("Not used while %s is on", Tr(WiiControlsOptionName()));

	GuiLayout * leftCol;
	GuiLayout * rightCol;
	if(showWii)
	{
		std::vector<WiiRowPlan> side[2], extra[2];
		PlanWiiColumns(layout, wiiControls, stageH - stageTop, side, extra);
		const auto wiiColumn = [this, wiiScheme, &wiiControls](const std::vector<WiiRowPlan>& plans, const std::vector<WiiRowPlan>& other, bool onLeft) -> GuiLayout& {
			GuiLayout& col = ui.column().gap(kRowGap).align(onLeft ? LAYOUT_ALIGN::END : LAYOUT_ALIGN::START);
			for(size_t i = 0; i < plans.size(); i++)
				col.add(wiiRow(plans[i], wiiScheme, onLeft, kSideW));
			if(!other.empty())
			{
				GuiLayout& heading = ui.row().size(kSideW, 0).pad(8, 5, 0, 1)
					.add(ui.label(Tr(wiiScheme == WiiScheme::WII && !wiiControls.derived ? "MOTION" : "NOTES")).fontSize(UI_FONT_SMALL - 2).muted());
				col.add(heading);
				for(size_t i = 0; i < other.size(); i++)
					col.add(wiiRow(other[i], wiiScheme, onLeft, kSideW));
			}
			return col;
		};
		leftCol = &wiiColumn(side[0], extra[0], true);
		rightCol = &wiiColumn(side[1], extra[1], false);
	}
	else
	{
		const auto column = [this](const std::vector<MapGroup>& groups, bool onLeft) -> GuiLayout& {
			GuiLayout& col = ui.column().gap(kRowGap).align(onLeft ? LAYOUT_ALIGN::END : LAYOUT_ALIGN::START);
			for(size_t g = 0; g < groups.size(); g++)
			{
				GuiLayout& heading = ui.row().size(kSideW, 0).pad(8, 5, 0, 1)
					.add(ui.label(Tr(groups[g].title)).fontSize(UI_FONT_SMALL - 2).muted());
				col.add(heading);
				for(size_t i = 0; i < groups[g].rows.size(); i++)
					col.add(bindingRow(groups[g].rows[i], onLeft, kSideW));
			}
			return col;
		};
		leftCol = &column(profile.left, true);
		rightCol = &column(profile.right, false);
	}

	// the drawing marks the control the selected row is mapped to, with a line to it
	ui.bind([this, h, &layout]{
		GuiCard * selected = nullptr;
		bool onLeft = true;
		int index = 0;
		for(size_t i = 0; i < rows.size(); i++)
			if(rows[i].second->isSelected())
			{
				selected = rows[i].second;
				index = rows[i].first;
				onLeft = rowOnLeft[i];
			}
		if(!selected)
		{
			if(!view->getHighlight().empty())
			{
				view->setHighlight(0);
				view->setLeader(false);
			}
			return;
		}
		if(!wiiSpots.empty())
		{
			// a game's own controls: the line says which controls it is on
			const std::vector<int>& spots = wiiSpots[(size_t)index];
			view->setHighlight(spots);
			view->setLeader(!spots.empty(), (float)(onLeft ? selected->getLeft() + selected->getWidth() : selected->getLeft()),
				(float)(selected->getTop() + selected->getHeight() / 2));
			return;
		}
		const int id = MappingSpotFor(layout, btnmap[h][index]);
		view->setHighlight(id);
		const float midY = (float)(selected->getTop() + selected->getHeight() / 2);
		const float edgeX = (float)(onLeft ? selected->getLeft() + selected->getWidth() : selected->getLeft());
		view->setLeader(id != 0, edgeX, midY);
	});

	GuiLayout& stage = ui.row().size(kContentW, stageH).pad(0, stageTop, 0, 0).gap(6).align(LAYOUT_ALIGN::CENTRE).justifyContent(LAYOUT_JUSTIFY::CENTRE)
		.add(*leftCol).add(*view).add(*rightCol);
	// the mappings are kept but not in use while the game's own Wii controls are: show them faded
	if(WiiControlsOverrideActive() && !showWii && !wiiMappingsApply)
		stage.setAlpha(kWiiDimAlpha);

	GuiTextButton& reset = ui.button(Tr("Reset")).size(48, kResetH).fitText(48, 9, 150).fontSize(UI_FONT_SMALL - 1).style(MenuKeycapStyle())
		.rounded(8.0f).litTextColor(MenuPaletteCurrent().text)
		.onFocus([this]{ status = "A puts these buttons back to their defaults"; });
	resetBtn = &reset;

	GuiLayout& content = ui.overlay().size(kContentW, stageH);

	// A game with Wii controls of its own gets the switch for them in the strip above the drawing (clear of Reset at the
	// other end, which is centred on it); other games never see it
	if(hasWii)
	{
		GuiLayout& resetBox = ui.column().pad(0, (kWiiBarH - kResetH) / 2, 0, 0).add(reset);
		content.add(resetBox, GuiItem().at(LAYOUT_ALIGN::END, LAYOUT_ALIGN::START));
		content.add(wiiToggle(EmuSettings.wiiControls, selectWii), GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::START));
	}
	else
		content.add(reset, GuiItem().at(LAYOUT_ALIGN::END, LAYOUT_ALIGN::START));
	content.add(stage, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::END));

	GuiCard& panel = ui.card().selectable(false).style(MenuPageCardStyle()).pad(kCardPad, kCardPad).size(kBodyW, 0);
	panel.add(content);
	GuiLayout& body = ui.column().size(sw, sh).pad(margin, MENU_BAR_TOP_H + 4, margin, MENU_BAR_BOTTOM_H + 4)
		.align(LAYOUT_ALIGN::CENTRE).justifyContent(LAYOUT_JUSTIFY::CENTRE);
	body.add(panel);

	// ---- footer: Back at the left (the logo is at the right)
	GuiTextButton& back = ui.button(Tr("Go Back")).size(110, 44).fontSize(20).style(MenuBarButtonStyle()).litTextColor(MenuPaletteCurrent().text);
	back.setTrigger(1, &cancel);
	backBtn = &back;
	GuiLayout& footer = ui.row().size(sw, MENU_BAR_BOTTOM_H).pad(margin, 0).gap(14).align(LAYOUT_ALIGN::CENTRE)
		.add(back);

	add(body, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::START));
	add(header, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::START));
	add(footer, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::END));
	setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	setPosition(0, 0);

	ui.attachTo(*this);
	// taking the focus steps the selection down from what is selected; starting at the chip lands on a row
	if(selectWii && wiiCard)
		wiiCard->select();
	else
		usingChipCard.select();
}

GuiPopover::Anchor MappingView::anchorForRow(int index)
{
	// the panel lines up with the side margin of the column the row is in, so it leaves the other side of the screen clear
	for(size_t i = 0; i < rows.size(); i++)
		if(rows[i].first == index)
			return GuiPopover::Anchor(*rows[i].second, rowOnLeft[i] ? GuiPopover::Anchor::LEFT_MARGIN : GuiPopover::Anchor::RIGHT_MARGIN);
	return GuiPopover::Anchor();
}

void MappingView::selectRow(int index)
{
	for(size_t i = 0; i < rows.size(); i++)
		if(rows[i].first == index)
		{
			rows[i].second->select();
			return;
		}
}

std::vector<PickerItem> HardwareItems(const std::vector<int>& list)
{
	std::vector<PickerItem> items;
	for(size_t i = 0; i < list.size(); i++)
	{
		PickerItem it;
		it.name = Tr(ctrlrName[list[i]]);
		it.current = list[i] == mapHardware;
		items.push_back(it);
	}
	return items;
}

} // namespace

void MappingScreenOpenedBy(int channel)
{
	openedHardware = HardwareInUse(channel);
}

int MappingScreen()
{
	// starts on the controller that opened it
	mapHardware = -1;
	ResolveHardware();
	openedHardware = -1;

	bool selectWii = false;		// keep the cursor on the Match Wii Controls switch after it rebuilds the screen
	while(true)
	{
		MappingView view(mapHardware, selectWii);
		selectWii = false;
		MenuScreenWindow screen(view);
		bool rebuild = false;

		while(!rebuild)
		{
			if(!MenuUpdateGui())
				return MENU_EXIT;

			if(view.backButton().getState() == STATE::CLICKED)
				return MENU_GAME;

			if(view.resetButton().getState() == STATE::CLICKED)
			{
				view.resetButton().resetState();
				if(MenuConfirm("Reset Mappings", "Are you sure that you want to reset your mappings?", "Yes", "No"))
					ResetControls(mapHardware);
			}

			bool wii;
			if(view.takeWiiChoice(wii))
			{
				EmuSettings.wiiControls = wii;
				selectWii = true;
				rebuild = true;
				continue;
			}

			int row;
			if(view.takeRow(row))
			{
				// a popover beside the row, over this screen, which stays drawn (dimmed) where it is
				const GuiPopover::Anchor anchor = view.anchorForRow(row);
				const MapRow * mapRow = FindRow(row);
				uint32_t pressed = 0;
				bool chosen;
				{
					MenuPageDisabled pageDisabled;
					chosen = MappingChoose(mapRow ? mapRow->name : "", mapHardware, btnmap[mapHardware][row],
						DefaultButton(mapHardware, row), anchor, pressed);
				}
				if(chosen)
					btnmap[mapHardware][row] = pressed;
				MenuMainWindow().changeFocus(&screen.window());
				screen.window().resetState();
				view.selectRow(row);
			}

			if(view.takePick() != MappingView::Pick::NONE)
			{
				// a popover over this screen, which stays drawn (dimmed) where it is
				const GuiPopover::Anchor anchor = view.anchorForChip();
				const std::vector<int> list = HardwareList();
				int chosen;
				{
					MenuPageDisabled pageDisabled;
					chosen = MenuPopover_Pick(Tr("Which controller is being mapped?"), HardwareItems(list), anchor);
				}
				MenuMainWindow().changeFocus(&screen.window());
				screen.window().resetState();

				if(chosen >= 0 && (size_t)chosen < list.size())
				{
					mapHardware = list[(size_t)chosen];
					rebuild = true;
				}
				else
					view.selectChip();
			}
		}
	}
}

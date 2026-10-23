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

#include "libgui/Gui.h"
#include "drivers/InputController.h"
#include "mappingui.h"
#include "mappingmodel.h"
#include "mappingchooser.h"
#include "controllerorder.h"
#include "controllerview.h"
#include "menupopover.h"
#include "menustyle.h"
#include "menu.h"
#include "input.h"
#include "button_mapping.h"
#include "vbagx.h"

extern GuiTextTranslator * textTranslator;

namespace {

// The hardware being mapped (-1 until chosen: the screen starts on the controller in use)
int mapHardware = -1;

const int kSideW = 172;			// a column of rows beside the drawing
const int kRowGap = 1;
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
	v.push_back(INPUT_HW_GAMECUBE);
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

// The hardware to start on: the controller in use on channel 0, chosen the way the controller strip chooses its icon
// (see controllerorder.h)
int DefaultHardware(const std::vector<int>& list)
{
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

std::string Tr(const char * s)
{
	return textTranslator ? std::string(textTranslator->getText(s)) : std::string(s);
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
std::string ValueText(int hw, int index)
{
	const char * name = MappingButtonName(hw, btnmap[hw][index]);
	return name ? std::string(name) : Tr("None");
}

// A chip that opens a popover: what it chooses in small capitals, what is chosen under it, an arrow pointing the way it opens
GuiCard& PickerChip(UiScope& ui, int width, const char * what, std::function<std::string()> value)
{
	GuiLabel& caption = ui.label(what).fontSize(UI_FONT_SMALL - 3).muted();
	GuiLabel& chosen = ui.label().fontSize(UI_FONT_SMALL + 1).bind(value);
	chosen.setMaxWidth(width - 20 - 16);
	GuiShape& arrow = ui.own<GuiShape>(10, 6);
	arrow.setArrow(SHAPE::TRIANGLE_DOWN, MenuBarArrowRest(), MenuBarArrowRest());
	GuiLayout& content = ui.row().size(width - 20, 0).gap(6).align(LAYOUT_ALIGN::CENTRE)
		.add(ui.column().gap(1).add(caption).add(chosen)).addSpace().add(arrow);
	GuiCard& chip = ui.card();
	chip.size(width, 40).style(MenuChipStyle()).corners(GuiCorners(10.0f)).pad(10, 3);
	chip.add(content);
	return chip;
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

		MappingView(int hw);

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
		GuiPopover::Anchor anchorForChip() { return GuiPopover::Anchor(*usingChip); }
		GuiPopover::Anchor anchorForRow(int index);
		void selectRow(int index);
		void selectChip() { usingChip->select(); }

	private:
		GuiCard& bindingRow(const MapRow& r, bool onLeft, int width);

		UiScope ui;
		int hardware;
		ControllerView * view;
		std::string status;
		std::vector<std::pair<int, GuiCard*>> rows;		// btnmap index, row
		std::vector<bool> rowOnLeft;
		GuiTextButton * backBtn;
		GuiTextButton * resetBtn;
		GuiCard * usingChip;
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

	GuiLabel& value = ui.label().fontSize(UI_FONT_SMALL + 1).nudgeY(2).accent().bind([hw, index]{ return ValueText(hw, index); });
	GuiLabel& label = ui.label(Tr(name).c_str()).fontSize(UI_FONT_SMALL + 2).nudgeY(2);

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
	row.onFocus([this, name]{ status = GuiFormat("A changes what %s does", Tr(name).c_str()); });
	row.onClick([this, index]{ requestedRow = index; rowRequested = true; });
	row.add(content);
	rows.push_back(std::make_pair(index, &row));
	rowOnLeft.push_back(onLeft);
	return row;
}

MappingView::MappingView(int hw) :
	hardware(hw), view(nullptr), status("Move over a button to see what it uses"),
	backBtn(nullptr), resetBtn(nullptr), usingChip(nullptr),
	rowRequested(false), requestedRow(0), pickRequest(Pick::NONE)
{
	const int sw = platform->getVideo()->getScreenWidth();
	const int sh = platform->getVideo()->getScreenHeight();
	const int margin = MENU_PAGE_MARGIN;
	setFixedSize(sw, sh);

	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();

	// ---- header: the page name with the hint under it, and the chip that chooses the hardware
	GuiLabel& title = ui.label(Tr("Button Mappings").c_str()).fontSize(UI_FONT_TITLE - 2).textColor(MenuChromeText());
	GuiLabel& hint = ui.label().fontSize(UI_FONT_SMALL + 2).textColor(MenuChromeTextMuted()).bind([this]{ return status; });
	hint.setMaxWidth(260);
	hint.setScroll(SCROLL::HORIZONTAL);
	GuiLayout& titleBlock = ui.column().gap(4).size(260, 0).add(title).add(hint);

	const int h = hardware;
	GuiCard& usingChipCard = PickerChip(ui, 196, "USING", [h]{ return Tr(ctrlrName[h]); });
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
	const float scale = MappingScale(layout, (float)drawW, (float)stageH - 24.0f);
	view = &ui.make<ControllerView>(layout, scale);
	// no text on the drawing: the rows name each control
	view->setLabelFilter([](int){ return false; });
	// lit while held, so pressing a button on the controller shows which control it is
	view->setLit([hw](int id){
		const InputPadData * pad = PadData();
		return pad && id > 0 && (pad->hw_buttons_h[hw] & (uint32_t)id) != 0;
	});

	const auto column = [this](const std::vector<MapGroup>& groups, bool onLeft) -> GuiLayout& {
		GuiLayout& col = ui.column().gap(kRowGap).align(onLeft ? LAYOUT_ALIGN::END : LAYOUT_ALIGN::START);
		for(size_t g = 0; g < groups.size(); g++)
		{
			GuiLayout& heading = ui.row().size(kSideW, 0).pad(8, 5, 0, 1)
				.add(ui.label(Tr(groups[g].title).c_str()).fontSize(UI_FONT_SMALL - 2).muted());
			col.add(heading);
			for(size_t i = 0; i < groups[g].rows.size(); i++)
				col.add(bindingRow(groups[g].rows[i], onLeft, kSideW));
		}
		return col;
	};
	GuiLayout& left = column(profile.left, true);
	GuiLayout& right = column(profile.right, false);

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
		const int id = MappingSpotFor(layout, btnmap[h][index]);
		view->setHighlight(id);
		const float midY = (float)(selected->getTop() + selected->getHeight() / 2);
		const float edgeX = (float)(onLeft ? selected->getLeft() + selected->getWidth() : selected->getLeft());
		view->setLeader(id != 0, edgeX, midY);
	});

	GuiLayout& stage = ui.row().size(kContentW, stageH).gap(6).align(LAYOUT_ALIGN::CENTRE).justifyContent(LAYOUT_JUSTIFY::CENTRE)
		.add(left).add(*view).add(right);

	GuiTextButton& reset = ui.button(Tr("Reset").c_str()).size(48, 22).fontSize(UI_FONT_SMALL - 1).style(MenuKeycapStyle())
		.rounded(8.0f).litTextColor(MenuPaletteCurrent().text)
		.onFocus([this]{ status = "A puts these buttons back to their defaults"; });
	resetBtn = &reset;

	GuiLayout& content = ui.overlay().size(kContentW, stageH);
	content.add(reset, GuiItem().at(LAYOUT_ALIGN::END, LAYOUT_ALIGN::START));
	content.add(stage, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::END));

	GuiCard& panel = ui.card().selectable(false).style(MenuPageCardStyle()).pad(kCardPad, kCardPad).size(kBodyW, 0);
	panel.add(content);
	GuiLayout& body = ui.column().size(sw, sh).pad(margin, MENU_BAR_TOP_H + 4, margin, MENU_BAR_BOTTOM_H + 4)
		.align(LAYOUT_ALIGN::CENTRE).justifyContent(LAYOUT_JUSTIFY::CENTRE);
	body.add(panel);

	// ---- footer: Back at the left (the logo is at the right)
	GuiTextButton& back = ui.button(Tr("Go Back").c_str()).size(110, 44).fontSize(20).style(MenuBarButtonStyle()).litTextColor(MenuPaletteCurrent().text);
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

int MappingScreen()
{
	// starts on the controller in use
	mapHardware = -1;
	ResolveHardware();

	while(true)
	{
		MappingView view(mapHardware);
		MenuScreenWindow screen(view);
		bool rebuild = false;

		while(!rebuild)
		{
			if(!MenuUpdateGui())
				return MENU_EXIT;

			if(view.backButton().getState() == STATE::CLICKED)
				return MENU_GAMESETTINGS;

			if(view.resetButton().getState() == STATE::CLICKED)
			{
				view.resetButton().resetState();
				if(MenuConfirm("Reset Mappings", "Are you sure that you want to reset your mappings?", "Yes", "No"))
					ResetControls(mapHardware);
			}

			int row;
			if(view.takeRow(row))
			{
				// a popover beside the row, over this screen, which stays drawn (dimmed) where it is
				const GuiPopover::Anchor anchor = view.anchorForRow(row);
				const MapRow * mapRow = FindRow(row);
				uint32_t pressed = 0;
				screen.setEnabled(false);
				const bool chosen = MappingChoose(mapRow ? mapRow->name : "", mapHardware, btnmap[mapHardware][row],
					DefaultButton(mapHardware, row), anchor, pressed);
				screen.setEnabled(true);
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
				screen.setEnabled(false);
				const std::vector<int> list = HardwareList();
				const int chosen = MenuPopover_Pick(Tr("Which controller is being mapped?").c_str(), HardwareItems(list), anchor);
				screen.setEnabled(true);
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

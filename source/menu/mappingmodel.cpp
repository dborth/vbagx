/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * mappingmodel.cpp
 *
 * See mappingmodel.h.
 ***************************************************************************/

#include "mappingmodel.h"
#include "button_mapping.h"
#include "input.h"

int MapProfile::rowCount() const
{
	int n = 0;
	for(size_t g = 0; g < left.size(); g++)
		n += (int)left[g].rows.size();
	for(size_t g = 0; g < right.size(); g++)
		n += (int)right[g].rows.size();
	return n;
}

// The indexes are the ones the old list used (and so the ones preferences.xml and ResetControls use)
static MapProfile BuildPad()
{
	MapProfile p;
	p.left.push_back({ "SHOULDER", { { "L", 8, MAP_ICON::L }, { "R", 9, MAP_ICON::R } } });
	p.left.push_back({ "D-PAD", { { "Up", 4, MAP_ICON::UP }, { "Down", 5, MAP_ICON::DOWN }, { "Left", 6, MAP_ICON::LEFT }, { "Right", 7, MAP_ICON::RIGHT } } });
	p.right.push_back({ "BUTTONS", { { "A", 1, MAP_ICON::A }, { "B", 0, MAP_ICON::B } } });
	p.right.push_back({ "SYSTEM", { { "Start", 3, MAP_ICON::START }, { "Select", 2, MAP_ICON::SELECT } } });
	return p;
}

const MapProfile& MappingProfile()
{
	static const MapProfile pad = BuildPad();
	return pad;
}

const ControllerLayout& MappingLayout(int hw, bool sideways)
{
	switch(hw)
	{
		case INPUT_HW_WIIMOTE: return sideways ? WiimoteSidewaysControllerLayout() : WiimoteControllerLayout();
		case INPUT_HW_NUNCHUK: return WiimoteNunchukControllerLayout();
		case INPUT_HW_CLASSIC: return ClassicControllerLayout();
		case INPUT_HW_WUPC: return WiiUProControllerLayout();
		case INPUT_HW_DRC: return GamePadControllerLayout();
		default: break;
	}
	return GameCubeControllerLayout();
}

float MappingScale(const ControllerLayout& layout, float maxW, float maxH)
{
	if(layout.width <= 0.0f || layout.height <= 0.0f)
		return 1.0f;
	const float sw = maxW / layout.width;
	const float sh = maxH / layout.height;
	return sw < sh ? sw : sh;
}

const char * MappingButtonName(int hw, uint32_t btn)
{
	if(btn == 0 || hw < 0 || hw >= (int)INPUT_HW_MAX)
		return nullptr;
	const CtrlrMap& def = ctrlr_def[hw];
	for(int i = 0; i < def.num_btns && i < 15; i++)
		if(def.map[i].btn == btn)
			return def.map[i].name;
	return nullptr;
}

int MappingSpotFor(const ControllerLayout& layout, uint32_t btn)
{
	if(btn == 0)
		return 0;
	return layout.find((int)btn) ? (int)btn : 0;
}

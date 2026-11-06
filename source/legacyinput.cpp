/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * legacyinput.cpp
 *
 * See legacyinput.h.
 ***************************************************************************/

#include <math.h>
#include <string.h>

#include "legacyinput.h"
#include "input.h"
#include "gameinput.h"
#include "drivers/InputController.h"

namespace {

struct ButtonPair { uint32_t input, legacy; };

const ButtonPair kWiimote[] = {
	{ INPUT_BTN_1, LW_BUTTON_1 }, { INPUT_BTN_2, LW_BUTTON_2 }, { INPUT_BTN_A, LW_BUTTON_A }, { INPUT_BTN_B, LW_BUTTON_B },
	{ INPUT_BTN_PLUS, LW_BUTTON_PLUS }, { INPUT_BTN_MINUS, LW_BUTTON_MINUS }, { INPUT_BTN_HOME, LW_BUTTON_HOME },
	{ INPUT_BTN_UP, LW_BUTTON_UP }, { INPUT_BTN_DOWN, LW_BUTTON_DOWN }, { INPUT_BTN_LEFT, LW_BUTTON_LEFT },
	{ INPUT_BTN_RIGHT, LW_BUTTON_RIGHT },
};

const ButtonPair kNunchuk[] = {
	{ INPUT_TRIGGER_L, LW_NUNCHUK_BUTTON_C }, { INPUT_TRIGGER_ZL, LW_NUNCHUK_BUTTON_Z },
};

const ButtonPair kClassic[] = {
	{ INPUT_BTN_A, LW_CLASSIC_BUTTON_A }, { INPUT_BTN_B, LW_CLASSIC_BUTTON_B }, { INPUT_BTN_X, LW_CLASSIC_BUTTON_X },
	{ INPUT_BTN_Y, LW_CLASSIC_BUTTON_Y }, { INPUT_TRIGGER_L, LW_CLASSIC_BUTTON_FULL_L },
	{ INPUT_TRIGGER_R, LW_CLASSIC_BUTTON_FULL_R }, { INPUT_TRIGGER_ZL, LW_CLASSIC_BUTTON_ZL },
	{ INPUT_TRIGGER_ZR, LW_CLASSIC_BUTTON_ZR }, { INPUT_BTN_PLUS, LW_CLASSIC_BUTTON_PLUS },
	{ INPUT_BTN_MINUS, LW_CLASSIC_BUTTON_MINUS }, { INPUT_BTN_HOME, LW_CLASSIC_BUTTON_HOME },
	{ INPUT_BTN_UP, LW_CLASSIC_BUTTON_UP }, { INPUT_BTN_DOWN, LW_CLASSIC_BUTTON_DOWN },
	{ INPUT_BTN_LEFT, LW_CLASSIC_BUTTON_LEFT }, { INPUT_BTN_RIGHT, LW_CLASSIC_BUTTON_RIGHT },
};

const ButtonPair kGameCube[] = {
	{ INPUT_BTN_A, LP_BUTTON_A }, { INPUT_BTN_B, LP_BUTTON_B }, { INPUT_BTN_X, LP_BUTTON_X }, { INPUT_BTN_Y, LP_BUTTON_Y },
	{ INPUT_BTN_PLUS, LP_BUTTON_START }, { INPUT_TRIGGER_L, LP_TRIGGER_L }, { INPUT_TRIGGER_R, LP_TRIGGER_R },
	{ INPUT_TRIGGER_ZR, LP_TRIGGER_Z }, { INPUT_BTN_UP, LP_BUTTON_UP }, { INPUT_BTN_DOWN, LP_BUTTON_DOWN },
	{ INPUT_BTN_LEFT, LP_BUTTON_LEFT }, { INPUT_BTN_RIGHT, LP_BUTTON_RIGHT },
};

template<size_t N>
uint32_t Translate(const ButtonPair (&table)[N], uint32_t buttons)
{
	uint32_t out = 0;
	for (size_t i = 0; i < N; i++)
		if (buttons & table[i].input)
			out |= table[i].legacy;
	return out;
}

const InputPadData * PadData(unsigned short pad)
{
	if (pad >= 4 || !controller[pad])
		return nullptr;
	return &controller[pad]->getPadData();
}

const int kClassicStyle[] = { INPUT_HW_CLASSIC, INPUT_HW_WUPC, INPUT_HW_DRC };

int8_t ToStick(float v)
{
	if (v > 1.0f) v = 1.0f;
	if (v < -1.0f) v = -1.0f;
	return (int8_t)(v * 127.0f);
}

LegacyStick MakeStick(float x, float y)
{
	LegacyStick s;
	s.x = x;
	s.y = y;
	s.mag = sqrtf(x * x + y * y);
	if (s.mag > 1.0f) s.mag = 1.0f;
	return s;
}

} // namespace

const LegacyWpad * LegacyWpad_Data(unsigned short pad)
{
	static LegacyWpad store[4];
	LegacyWpad& wp = store[pad < 4 ? pad : 0];
	memset(&wp, 0, sizeof(wp));
	wp.exp.type = LW_EXP_NONE;

	const InputPadData * data = PadData(pad);
	if (!data)
		return &wp;

	if (data->hw_connected[INPUT_HW_WIIMOTE])
	{
		wp.btns_h = Translate(kWiimote, data->hw_buttons_h[INPUT_HW_WIIMOTE]);
		wp.btns_d = Translate(kWiimote, data->hw_buttons_d[INPUT_HW_WIIMOTE]);
		wp.btns_u = Translate(kWiimote, data->hw_buttons_r[INPUT_HW_WIIMOTE]);
		wp.gforce.x = data->hw_gforceX[INPUT_HW_WIIMOTE];
		wp.gforce.y = data->hw_gforceY[INPUT_HW_WIIMOTE];
		wp.gforce.z = data->hw_gforceZ[INPUT_HW_WIIMOTE];
		wp.orient.pitch = data->hw_pitch[INPUT_HW_WIIMOTE];
		wp.orient.roll = data->hw_roll[INPUT_HW_WIIMOTE];
		wp.orient.yaw = data->hw_yaw[INPUT_HW_WIIMOTE];
	}

	if (data->hw_connected[INPUT_HW_NUNCHUK])
	{
		wp.exp.type = LW_EXP_NUNCHUK;
		// the Nunchuk's own buttons; the Wii Remote's come from the Wii Remote
		wp.btns_h |= Translate(kNunchuk, data->hw_buttons_h[INPUT_HW_NUNCHUK]);
		wp.btns_d |= Translate(kNunchuk, data->hw_buttons_d[INPUT_HW_NUNCHUK]);
		wp.btns_u |= Translate(kNunchuk, data->hw_buttons_r[INPUT_HW_NUNCHUK]);
		wp.exp.nunchuk.gforce.x = data->hw_gforceX[INPUT_HW_NUNCHUK];
		wp.exp.nunchuk.gforce.y = data->hw_gforceY[INPUT_HW_NUNCHUK];
		wp.exp.nunchuk.gforce.z = data->hw_gforceZ[INPUT_HW_NUNCHUK];
		wp.exp.nunchuk.orient.pitch = data->hw_pitch[INPUT_HW_NUNCHUK];
		wp.exp.nunchuk.orient.roll = data->hw_roll[INPUT_HW_NUNCHUK];
		wp.exp.nunchuk.orient.yaw = data->hw_yaw[INPUT_HW_NUNCHUK];
		wp.exp.nunchuk.js = MakeStick(data->hw_stickX[INPUT_HW_NUNCHUK], data->hw_stickY[INPUT_HW_NUNCHUK]);
	}
	else
	{
		// Classic Controller, Pro Controller and GamePad: one Classic Controller
		bool classic = false;
		uint32_t held = 0, down = 0, up = 0;
		for (size_t i = 0; i < sizeof(kClassicStyle) / sizeof(kClassicStyle[0]); i++)
		{
			const int hw = kClassicStyle[i];
			if (!data->hw_connected[hw])
				continue;
			if (!classic)
			{
				wp.exp.classic.ljs = MakeStick(data->hw_stickX[hw], data->hw_stickY[hw]);
				wp.exp.classic.rjs = MakeStick(data->hw_substickX[hw], data->hw_substickY[hw]);
			}
			classic = true;
			held |= data->hw_buttons_h[hw];
			down |= data->hw_buttons_d[hw];
			up |= data->hw_buttons_r[hw];
		}
		if (classic)
		{
			wp.exp.type = LW_EXP_CLASSIC;
			wp.btns_h |= Translate(kClassic, held);
			wp.btns_d |= Translate(kClassic, down);
			wp.btns_u |= Translate(kClassic, up);
			wp.exp.classic.ls_raw = (held & INPUT_TRIGGER_L) ? 0x1F : 0;
			wp.exp.classic.rs_raw = (held & INPUT_TRIGGER_R) ? 0x1F : 0;
		}
	}
	return &wp;
}

int8_t LegacyWpad_StickX(unsigned short pad, int stick)
{
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	if (wp->exp.type == LW_EXP_NUNCHUK) return stick == 0 ? ToStick(wp->exp.nunchuk.js.x) : 0;
	if (wp->exp.type == LW_EXP_CLASSIC) return ToStick(stick == 0 ? wp->exp.classic.ljs.x : wp->exp.classic.rjs.x);
	return 0;
}

int8_t LegacyWpad_StickY(unsigned short pad, int stick)
{
	const LegacyWpad * wp = LegacyWpad_Data(pad);
	if (wp->exp.type == LW_EXP_NUNCHUK) return stick == 0 ? ToStick(wp->exp.nunchuk.js.y) : 0;
	if (wp->exp.type == LW_EXP_CLASSIC) return ToStick(stick == 0 ? wp->exp.classic.ljs.y : wp->exp.classic.rjs.y);
	return 0;
}

uint32_t LegacyPad_ButtonsHeld(unsigned short pad)
{
	const InputPadData * data = PadData(pad);
	return data && data->hw_connected[INPUT_HW_GAMECUBE] ? Translate(kGameCube, data->hw_buttons_h[INPUT_HW_GAMECUBE]) : 0;
}

uint32_t LegacyPad_ButtonsDown(unsigned short pad)
{
	const InputPadData * data = PadData(pad);
	return data && data->hw_connected[INPUT_HW_GAMECUBE] ? Translate(kGameCube, data->hw_buttons_d[INPUT_HW_GAMECUBE]) : 0;
}

uint32_t LegacyPad_ButtonsUp(unsigned short pad)
{
	const InputPadData * data = PadData(pad);
	return data && data->hw_connected[INPUT_HW_GAMECUBE] ? Translate(kGameCube, data->hw_buttons_r[INPUT_HW_GAMECUBE]) : 0;
}

int8_t LegacyPad_StickX(unsigned short pad)
{
	const InputPadData * data = PadData(pad);
	return data && data->hw_connected[INPUT_HW_GAMECUBE] ? ToStick(data->hw_stickX[INPUT_HW_GAMECUBE]) : 0;
}

int8_t LegacyPad_StickY(unsigned short pad)
{
	const InputPadData * data = PadData(pad);
	return data && data->hw_connected[INPUT_HW_GAMECUBE] ? ToStick(data->hw_stickY[INPUT_HW_GAMECUBE]) : 0;
}

int8_t LegacyPad_SubStickX(unsigned short pad)
{
	const InputPadData * data = PadData(pad);
	return data && data->hw_connected[INPUT_HW_GAMECUBE] ? ToStick(data->hw_substickX[INPUT_HW_GAMECUBE]) : 0;
}

int8_t LegacyPad_SubStickY(unsigned short pad)
{
	const InputPadData * data = PadData(pad);
	return data && data->hw_connected[INPUT_HW_GAMECUBE] ? ToStick(data->hw_substickY[INPUT_HW_GAMECUBE]) : 0;
}

uint32_t DecodeGamecube(unsigned short pad)
{
	const InputPadData * data = PadData(pad);
	if (!data || !data->hw_connected[INPUT_HW_GAMECUBE])
		return 0;
	return MappedButtons(INPUT_HW_GAMECUBE, data->hw_buttons_h[INPUT_HW_GAMECUBE]);
}

uint32_t DecodeWiimote(unsigned short pad)
{
	const InputPadData * data = PadData(pad);
	if (!data || !data->hw_connected[INPUT_HW_WIIMOTE])
		return 0;
	// only when nothing is plugged into it
	if (data->hw_connected[INPUT_HW_NUNCHUK] || data->hw_connected[INPUT_HW_CLASSIC] || data->hw_connected[INPUT_HW_WUPC])
		return 0;
	return MappedButtons(INPUT_HW_WIIMOTE, data->hw_buttons_h[INPUT_HW_WIIMOTE]);
}

uint32_t DecodeClassic(unsigned short pad)
{
	const InputPadData * data = PadData(pad);
	if (!data)
		return 0;
	uint32_t J = 0;
	for (size_t i = 0; i < sizeof(kClassicStyle) / sizeof(kClassicStyle[0]); i++)
	{
		const int hw = kClassicStyle[i];
		if (data->hw_connected[hw])
			J |= MappedButtons(hw, data->hw_buttons_h[hw]);
	}
	return J;
}

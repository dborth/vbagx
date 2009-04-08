/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * button_mapping.c
 *
 * Controller button mapping
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <unistd.h>
#include <wiiuse/wpad.h>

#include "button_mapping.h"

/****************************************************************************
 * Controller Button Descriptions:
 * used for identifying which buttons have been pressed when configuring
 * and for displaying the name of said button
 ***************************************************************************/

CtrlrMap ctrlr_def[5] = {
// Gamecube controller btn def
{
	CTRLR_GCPAD,
	13,
	{
		{PAD_BUTTON_DOWN, "DOWN"},
		{PAD_BUTTON_UP, "UP"},
		{PAD_BUTTON_LEFT, "LEFT"},
		{PAD_BUTTON_RIGHT, "RIGHT"},
		{PAD_BUTTON_A, "A"},
		{PAD_BUTTON_B, "B"},
		{PAD_BUTTON_X, "X"},
		{PAD_BUTTON_Y, "Y"},
		{PAD_BUTTON_MENU, "START"},
		{PAD_BUTTON_START, "START"},
		{PAD_TRIGGER_L, "L"},
		{PAD_TRIGGER_R, "R"},
		{PAD_TRIGGER_Z, "Z"},
		{0, ""},
		{0, ""}
	}
},
// Wiimote btn def
{
	CTRLR_WIIMOTE,
	11,
	{
		{WPAD_BUTTON_DOWN, "DOWN"},
		{WPAD_BUTTON_UP, "UP"},
		{WPAD_BUTTON_LEFT, "LEFT"},
		{WPAD_BUTTON_RIGHT, "RIGHT"},
		{WPAD_BUTTON_A, "A"},
		{WPAD_BUTTON_B, "B"},
		{WPAD_BUTTON_1, "1"},
		{WPAD_BUTTON_2, "2"},
		{WPAD_BUTTON_PLUS, "PLUS"},
		{WPAD_BUTTON_MINUS, "MINUS"},
		{WPAD_BUTTON_HOME, "HOME"},
		{0, ""},
		{0, ""},
		{0, ""},
		{0, ""}
	}
},
// Nunchuk btn def
{
	CTRLR_NUNCHUK,
	13,
	{
		{WPAD_BUTTON_DOWN, "DOWN"},
		{WPAD_BUTTON_UP, "UP"},
		{WPAD_BUTTON_LEFT, "LEFT"},
		{WPAD_BUTTON_RIGHT, "RIGHT"},
		{WPAD_BUTTON_A, "A"},
		{WPAD_BUTTON_B, "B"},
		{WPAD_BUTTON_1, "1"},
		{WPAD_BUTTON_2, "2"},
		{WPAD_BUTTON_PLUS, "PLUS"},
		{WPAD_BUTTON_MINUS, "MINUS"},
		{WPAD_BUTTON_HOME, "HOME"},
		{WPAD_NUNCHUK_BUTTON_Z, "Z"},
		{WPAD_NUNCHUK_BUTTON_C, "C"},
		{0, ""},
		{0, ""}
	}
},
// Classic btn def
{
	CTRLR_CLASSIC,
	15,
	{
		{WPAD_CLASSIC_BUTTON_DOWN, "DOWN"},
		{WPAD_CLASSIC_BUTTON_UP, "UP"},
		{WPAD_CLASSIC_BUTTON_LEFT, "LEFT"},
		{WPAD_CLASSIC_BUTTON_RIGHT, "RIGHT"},
		{WPAD_CLASSIC_BUTTON_A, "A"},
		{WPAD_CLASSIC_BUTTON_B, "B"},
		{WPAD_CLASSIC_BUTTON_X, "X"},
		{WPAD_CLASSIC_BUTTON_Y, "Y"},
		{WPAD_CLASSIC_BUTTON_PLUS, "PLUS"},
		{WPAD_CLASSIC_BUTTON_MINUS, "MINUS"},
		{WPAD_CLASSIC_BUTTON_HOME, "HOME"},
		{WPAD_CLASSIC_BUTTON_FULL_L, "L"},
		{WPAD_CLASSIC_BUTTON_FULL_R, "R"},
		{WPAD_CLASSIC_BUTTON_ZL, "ZL"},
		{WPAD_CLASSIC_BUTTON_ZR, "ZR"}
	}
},
// Keyboard btn def, see http://www.usb.org/developers/devclass_docs/Hut1_12.pdf
{
	CTRLR_KEYBOARD,
	150,
	{
		{4, "A"},
		{5, "B"},
		{6, "C"},
		{7, "D"},
		{8, "E"},
		{9, "F"},
		{10, "G"},
		{11, "H"},
		{12, "I"},
		{13, "J"},
		{14, "K"},
		{15, "L"},
		{16, "M"},
		{17, "N"},
		{18, "O"},
		{19, "P"},
		{20, "Q"},
		{21, "R"},
		{22, "S"},
		{23, "T"},
		{24, "U"},
		{25, "V"},
		{26, "W"},
		{27, "X"},
		{28, "Y"},
		{29, "Z"},
		{30, "1"},
		{31, "2"},
		{32, "3"},
		{33, "4"},
		{34, "5"},
		{35, "6"},
		{36, "7"},
		{37, "8"},
		{38, "9"},
		{39, "0"},
		{40, "ENTER"},
		{41, "ESC"},
		{42, "BKSP"},
		{43, "TAB"},
		{44, "SPACE"},
		{45, "-"},
		{46, "="},
		{47, "["},
		{48, "]"},
		{49, "\\"},
		{50, "#"},
		{51, ";"},
		{52, "'"},
		{53, "`"},
		{54, ","},
		{55, "."},
		{56, "/"},
		{57, "CAPLK"},
		{58, "F1"},
		{59, "F2"},
		{60, "F3"},
		{61, "F4"},
		{62, "F5"},
		{63, "F6"},
		{64, "F7"},
		{65, "F8"},
		{66, "F9"},
		{67, "F10"},
		{68, "F11"},
		{69, "F12"},
		{70, "PRTSC"},
		{71, "SCRLK"},
		{72, "PAUSE"},
		{73, "INS"},
		{74, "HOME"},
		{75, "PGUP"},
		{76, "DEL"},
		{77, "END"},
		{78, "PGDN"},
		{79, "RIGHT"},
		{80, "LEFT"},
		{81, "DOWN"},
		{82, "UP"},
		{83, "NUMLK"},
		{84, "NP/"},
		{85, "NP*"},
		{86, "NP-"},
		{87, "NP+"},
		{88, "NPENT"},
		{89, "NP1"},
		{90, "NP2"},
		{91, "NP3"},
		{92, "NP4"},
		{93, "NP5"},
		{94, "NP6"},
		{95, "NP7"},
		{96, "NP8"},
		{97, "NP9"},
		{98, "NP0"},
		{99, "NP."},
		{100, "\\|"},
		{101, "APP"},
		{102, "POWER"},
		{103, "NP="},
		{104, "F13"},
		{105, "F14"},
		{106, "F15"},
		{107, "F16"},
		{108, "F17"},
		{109, "F18"},
		{110, "F19"},
		{111, "F20"},
		{112, "F21"},
		{113, "F22"},
		{114, "F23"},
		{115, "F24"},

		{127, "MUTE"},
		{128, "VOL+"},
		{129, "VOL-"},
	
		{144, "HANGL"},
		{145, "HANJA"},
		{146, "KATA"},
		{147, "HIRA"},
		
		{224, "LCTRL"},
		{225, "LSHFT"},
		{226, "LALT"},
		{227, "LWIN"},
		{228, "RCTRL"},
		{229, "RSHFT"},
		{230, "RALT"},
		{231, "RWIN"},
		{232, "MOUSEL"},
		{233, "MOUSER"},
		{234, "MOUSEM"}
	}
}
};

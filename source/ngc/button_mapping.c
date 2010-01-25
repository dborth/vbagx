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
#include "wiiusbsupport.h"

/****************************************************************************
 * Controller Button Descriptions:
 * used for identifying which buttons have been pressed when configuring
 * and for displaying the name of said button
 ***************************************************************************/

CtrlrMap ctrlr_def[5] = {
// Gamecube controller btn def
{
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
	},
	13,
	CTRLR_GCPAD
},
// Wiimote btn def
{
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
	},
	11,
	CTRLR_WIIMOTE
},
// Nunchuk btn def
{
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
	},
	13,
	CTRLR_NUNCHUK
},
// Classic btn def
{
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
	},
	15,
	CTRLR_CLASSIC
},
// Keyboard btn def
{
	{
		{KS_A, "A"},
		{KS_B, "B"},
		{KS_C, "C"},
		{KS_D, "D"},
		{KS_E, "E"},
		{KS_F, "F"},
		{KS_G, "G"},
		{KS_H, "H"},
		{KS_I, "I"},
		{KS_J, "J"},
		{KS_K, "K"},
		{KS_L, "L"},
		{KS_M, "M"},
		{KS_N, "N"},
		{KS_O, "O"},
		{KS_P, "P"},
		{KS_Q, "Q"},
		{KS_R, "R"},
		{KS_S, "S"},
		{KS_T, "T"},
		{KS_U, "U"},
		{KS_V, "V"},
		{KS_W, "W"},
		{KS_X, "X"},
		{KS_Y, "Y"},
		{KS_Z, "Z"},
		{KS_1, "1"},
		{KS_2, "2"},
		{KS_3, "3"},
		{KS_4, "4"},
		{KS_5, "5"},
		{KS_6, "6"},
		{KS_7, "7"},
		{KS_8, "8"},
		{KS_9, "9"},
		{KS_0, "0"},
		{KS_Return, "ENTER"},
		{KS_Escape, "ESC"},
		{KS_BackSpace, "BKSP"},
		{KS_Tab, "TAB"},
		{KS_space, "SPACE"},
		{KS_F1, "F1"},
		{KS_F2, "F2"},
		{KS_F3, "F3"},
		{KS_F4, "F4"},
		{KS_F5, "F5"},
		{KS_F6, "F6"},
		{KS_F7, "F7"},
		{KS_F8, "F8"},
		{KS_F9, "F9"},
		{KS_F10, "F10"},
		{KS_F11, "F11"},
		{KS_F12, "F12"},
		{KS_Right, "RIGHT"},
		{KS_Left, "LEFT"},
		{KS_Down, "DOWN"},
		{KS_Up, "UP"},
		{KS_Control_L, "LCTRL"},
		{KS_Shift_L, "LSHFT"},
		{KS_Alt_L, "LALT"},
		{KS_Control_R, "RCTRL"},
		{KS_Shift_R, "RSHFT"},
		{KS_Alt_R, "RALT"},
		{MOUSEL, "MOUSEL"},
		{MOUSER, "MOUSER"},
		{MOUSEM, "MOUSEM"}
	},
	66,
	CTRLR_KEYBOARD
}
};

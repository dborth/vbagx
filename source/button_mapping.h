/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * button_mapping.h
 *
 * Controller button mapping
 ***************************************************************************/

#ifndef BTN_MAP_H
#define BTN_MAP_H

enum {
	CTRLR_NONE = -1,
	CTRLR_GCPAD,
	CTRLR_WIIMOTE,
	CTRLR_NUNCHUK,
	CTRLR_CLASSIC
};

const char ctrlrName[4][20] =
{ "GameCube Controller", "Wiimote", "Nunchuk + Wiimote", "Classic Controller" };

typedef struct _btn_map {
	u32 btn;					// button 'id'
	char* name;					// button name
} BtnMap;

typedef struct _ctrlr_map {
	BtnMap map[15];				// controller button map
	int num_btns;				// number of buttons on the controller
	u16 type;					// controller type
} CtrlrMap;

extern CtrlrMap ctrlr_def[4];

#endif

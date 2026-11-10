/****************************************************************************
 * Platform Abstraction Layer (Wii driver)
 * Daryl Borth 2026
 * WiiUsbPads.cpp
 ***************************************************************************/

#include <gccore.h>

#include "WiiUsbPads.h"
#include "usbinput.h"
#include "tablet.h"

/****************************************************************************
 * Hardware Mapping Helpers
 * Each device's native buttons to our generic UI masks
 ***************************************************************************/
static uint32_t MapTabletToGeneric(uint32_t tablet_btns) {
	uint32_t mask = INPUT_BTN_NONE;
	if (tablet_btns & TABLET_BUTTON_TIP) mask |= INPUT_BTN_A;
	if (tablet_btns & TABLET_BUTTON_BARREL) mask |= INPUT_BTN_B;
	if (tablet_btns & TABLET_BUTTON_BARREL2) mask |= INPUT_BTN_X;
	return mask;
}

/****************************************************************************
 * Scan
 ***************************************************************************/
void WiiUsbPads::scan() {
	// Discovery first: it attaches the devices the ScanPads() calls below read from
	UsbInput_Scan();
	Tablet_ScanPads();
}

/****************************************************************************
 * Read
 ***************************************************************************/

// USB pen tablet, on channel 0, in the USB mouse's hardware profile: the tip is the left button
// (A), the lower and upper side switches the right (B) and middle (X) ones. While the pen is in
// range it owns the pointer, the whole tablet mapped onto the screen.
void WiiUsbPads::readPointer(int channel, int width, int height, InputPadData & pad) {
	if (channel != 0 || !Tablet_Connected())
		return;

	pad.hw_connected[INPUT_HW_USBMOUSE] = true;
	pad.hw_buttons_d[INPUT_HW_USBMOUSE] |= MapTabletToGeneric(Tablet_ButtonsDown());
	pad.hw_buttons_h[INPUT_HW_USBMOUSE] |= MapTabletToGeneric(Tablet_ButtonsHeld());
	pad.hw_buttons_r[INPUT_HW_USBMOUSE] |= MapTabletToGeneric(Tablet_ButtonsUp());

	if (Tablet_InRange()) {
		pad.validPointer = true;
		pad.isTouch = false;
		pad.cursor_x = Tablet_X() * (width - 1);
		pad.cursor_y = Tablet_Y() * (height - 1);
		pad.cursor_angle = 0.0f;
	}
}

/****************************************************************************
 * Platform Abstraction Layer (Wii driver)
 * Daryl Borth 2026
 * WiiUsbPads.h
 *
 * The USB input devices read through usbinput.c (here, the pen tablet) as
 * one unit for WiiInputDriver. The device drivers themselves (tablet.c and
 * the shared discovery in usbinput.c) stay plain C; this is the only place
 * that knows their APIs, and where each device's native buttons are mapped
 * to the generic input masks.
 *
 * To add a device: write its C driver and register it in usbinput.c (the
 * checklist is at the top of usbinput.h), then add its scan and read here.
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include "../../../InputData.h"

class WiiUsbPads
{
	public:
		//! Discovers/attaches devices and latches their state. Once per frame, before readPointer().
		void scan();

		//! Adds the pen tablet to `pad` on channel 0, in the USB mouse's profile, and
		//! its pen as the pointer on a `width` x `height` screen while in range. Called after the
		//! Wiimote, so the pen takes the pointer over from the IR.
		void readPointer(int channel, int width, int height, InputPadData & pad);
};

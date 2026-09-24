/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * EmulatorVideoDriver.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include "VideoDriver.h"

class EmulatorVideoDriver
{
	public:
		virtual ~EmulatorVideoDriver() = default;

		virtual void init(VideoDriver* videoDriver) = 0;
		virtual void resetVideo() = 0;
		virtual void initFPSFontData() = 0;
		virtual void presentFrame(int width, int height) = 0;

		//! Copies whatever this driver needs out of its live frame source,
		//! into storage it owns itself, so a later readFrameRGB24() call
		//! still has something valid to read even if the live source gets
		//! invalidated/repurposed in between.
		virtual void snapshotFrame() = 0;

		//! Converts the width x height frame most recently captured by
		//! snapshotFrame() into packed RGB24, written to dst
		//! (width*height*3 bytes, tightly packed, no dst padding).
		virtual void readFrameRGB24(int width, int height, uint8_t* dst) = 0;

		//! Sets the initial console dimensions, before the first presentFrame() call
		virtual void renderInit(int width, int height) = 0;

		//! Maps a UI-canvas pointer position (IR pointer / touch, in the same canvas
		//! coordinates as InputPadData::cursor_x/y) to a normalized position (0..1 on
		//! each axis, clamped) within the game picture, following its actual on-screen
		//! placement (aspect, zoom, fixed scale, shift). Returns false until the
		//! placement is known.
		virtual bool mapPointerToUnit(float canvasX, float canvasY, float* u, float* v)
		{
			(void)canvasX; (void)canvasY; (void)u; (void)v;
			return false;
		}
};

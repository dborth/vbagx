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

		//! Converts width x height pixels of src (in whatever live texture
		//! format/layout this driver's presentFrame() produces) into packed
		//! RGB24 at dst. src is caller-supplied rather than implicitly "the
		//! current frame" so callers can snapshot a buffer before it's
		//! invalidated/repurposed (e.g. before switching memory modes).
		virtual void readFrameRGB24(const void* src, int width, int height, uint8_t* dst) = 0;

		//! Sets the initial console dimensions, before the first presentFrame() call
		void renderInit(int width, int height);
};

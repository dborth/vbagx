/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * OgcEmulatorVideo.h
 ***************************************************************************/
#pragma once

#include <gccore.h>
#include <stdint.h>
#include "../EmulatorVideoDriver.h"

class OgcVideoDriver;

class OgcEmulatorVideo : public EmulatorVideoDriver
{
	public:
		OgcEmulatorVideo() : videoDriver(nullptr) {}

		void init(VideoDriver* videoDriver) override;
		void resetVideo() override;
		void presentFrame(int width, int height) override;
		void readFrameRGB24(uint8_t* dst) override;

		//! Sets the initial console dimensions, before the first presentFrame() call
		void renderInit(int width, int height);

		//! Loads the FPS overlay font into texture memory. Must be called at startup.
		void initFPSFontData();

		//! Un-swizzles a 4x4-tiled GX_TF_RGB5A3 texture into packed RGB24
		void untileRGB5A3ToRGB24(const void * tiledTexture, int width, int height, uint8_t* dst);

	private:
		void initScanlineTexture();
		void setupScanlineFilterTEV();
		void initFPSFontTexture();
		void drawInit();
		void configureTEV();
		void drawSquare();
		void drawCursor();
		void drawFps(const char* str, float x, float y);
		void recalculateScaling();
		OgcVideoDriver* videoDriver;
};

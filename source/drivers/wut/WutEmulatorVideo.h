/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * WutEmulatorVideo.h
 *
 * EmulatorVideoDriver implementation for Wii U: uploads the raw GB/GBA
 * framebuffer into a linear GX2 texture and draws it with the shared
 * Texture2DShader.
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <gx2/sampler.h>
#include <gx2/texture.h>
#include "../EmulatorVideoDriver.h"

class WutVideoDriver;

class WutEmulatorVideo : public EmulatorVideoDriver
{
	public:
		WutEmulatorVideo();
		~WutEmulatorVideo() override;

		void init(VideoDriver* videoDriver) override;
		void resetVideo() override;
		void presentFrame(int width, int height) override;
		void readFrameRGB24(const void* src, int width, int height, uint8_t* dst) override;

		//! Sets the initial console dimensions, before the first presentFrame() call
		void renderInit(int width, int height);

		//! Loads the FPS overlay font into texture memory. Must be called at startup.
		void initFPSFontData();

	private:
		void rebuildTexture(int width, int height);
		void destroyTexture();
		void uploadFrame();
		void drawQuad();

		WutVideoDriver* videoDriver;

		GX2Texture* texture;
		GX2Sampler sampler;

		int vwidth, vheight;
		int oldvwidth, oldvheight;
		int checkVideo;
		uint32_t prevRenderedFrameCount;

		// On-screen placement of the game quad, in design-canvas pixels
		// (top-left x/y, size w/h) - recomputed by resetVideo().
		float quadX, quadY, quadWidth, quadHeight;
};

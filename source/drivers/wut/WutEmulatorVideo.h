/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * WutEmulatorVideo.h
 *
 * EmulatorVideoDriver implementation for Wii U: uploads the raw GB/GBA
 * framebuffer into a linear GX2 texture and draws it with the shared
 * Texture2DShader. Also owns the FPS overlay and pointer-cursor overlay,
 * both drawn as extra quads through the same shader.
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <gx2/sampler.h>
#include <gx2/texture.h>
#include "../EmulatorVideoDriver.h"

class WutVideoDriver;
class GuiImageData;

class WutEmulatorVideo : public EmulatorVideoDriver
{
	public:
		WutEmulatorVideo();
		~WutEmulatorVideo() override;

		void init(VideoDriver* videoDriver) override;
		void resetVideo() override;
		void presentFrame(int width, int height) override;
		void snapshotFrame() override;
		void readFrameRGB24(int width, int height, uint8_t* dst) override;

		//! Sets the initial console dimensions, before the first presentFrame() call
		void renderInit(int width, int height);

		//! Loads the FPS overlay font into its own texture.
		//! Must be called at startup, before the app-level memory mode ever
		//! switches away from menu
		void initFPSFontData();

	private:
		void rebuildTexture(int width, int height);
		void destroyTexture();
		//! Converts and blits the raw gbWidth x gbHeight console framebuffer
		//! (RGB555, from the shared 'pix' buffer) into the live linear GX2
		//! texture, compositing gameBorder's RGBA8 pixels first if active.
		void uploadFrame(int gbWidth, int gbHeight);
		void drawQuad();
		//! Draws EmuSettings.DisplayFrameRate's "FPS: NN.N" readout, one
		//! glyph-quad at a time out of fpsFont's character atlas.
		void drawFpsOverlay();
		//! Draws the shared menu pointer cursor at CursorX/CursorY when a 
		//! pointer device has it active.
		void drawCursorOverlay();

		WutVideoDriver* videoDriver;

		GX2Texture* texture;
		GX2Sampler sampler;    // game frame: linear or point, per EmuSettings.videoBilinearFilter
		GX2Sampler fontSampler; // FPS glyphs: always point-sampled, for crisp pixel text
		GX2Sampler uiSampler;   // cursor: always linear

		int vwidth, vheight;
		int oldvwidth, oldvheight;
		int checkVideo;

		// On-screen placement of the game quad, in design-canvas pixels
		// (top-left x/y, size w/h) - recomputed by resetVideo().
		float quadX, quadY, quadWidth, quadHeight;

		// FPS overlay. fpsFont owns its own GX2Texture (allocated through
		// the normal WutImageRenderer path, from the OS default heap - NOT
		// game-mode mspace memory). fpsGlyphTexCoords is a
		// small GX2-visible scratch buffer this class owns, used to
		// temporarily override Texture2DShader's texcoord attribute buffer
		// with a single glyph's UV sub-rect for one draw call at a time -
		// see drawFpsOverlay().
		GuiImageData* fpsFont;
		float* fpsGlyphTexCoords;
		uint32_t lastFpsTime;
		char fpsStr[16];

		// One-shot - allocated by snapshotFrame(), consumed and freed by
		// the next readFrameRGB24() call.
		uint8_t* screenshotSnapshot;
		int screenshotWidth, screenshotHeight;
		uint32_t screenshotPitch; // texels/row, RGBA8, as snapshotted
};

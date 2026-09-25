/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutVideoDriver.h
 ***************************************************************************/
#pragma once

#include <coreinit/time.h>
#include <gx2/sampler.h>
#include <gx2/texture.h>
#include "../VideoDriver.h"

//!The two physical render targets every Wii U frame is submitted to.
//!Their pixel dimensions are not fixed: the TV follows the console's output
//!setting (480p/720p/1080p), the GamePad is always 854x480. See
//!WutVideoDriver::getTargetWidth()/getTargetHeight().
enum class OutputTarget
{
	TV = 0,
	DRC = 1
};

static const int OUTPUT_TARGET_COUNT = 2;

class WutEmulatorVideo;

//!Wii U VideoDriver: GX2 + libwhb's WHBGfx* helpers. Every draw pass runs
//!twice per frame - once for the TV, once for the GamePad - so the same
//!UI always reaches both screens; there's no separate dual-display mode.
class WutVideoDriver : public VideoDriver
{
	public:
		WutVideoDriver();
		~WutVideoDriver() override;

		void init(int width, int height) override;
		void shutdown() override;
		void renderMenu() override;
		void startMenuVideo() override;
		void clearScreen(const PixelColor& color) override;

		int getScreenWidth() const override { return screenWidth; }
		int getScreenHeight() const override { return screenHeight; }
		uint32_t getFrameTimer() override;
		void setFrameTimer(uint32_t _frameTimer) override;

		int getRefreshRate() const override;
		float getDeltaTime() const override;
		float getUIScale() const override { return uiScale; }

		//!Physical pixel size of a render target (the TV follows the console's
		//!output setting, the GamePad is always 854x480). Unrelated to the
		//!design canvas returned by getScreenWidth()/getScreenHeight(), which
		//!is stretched onto each target independently per axis.
		int getTargetWidth(OutputTarget target) const { return targetWidth[(int)target]; }
		int getTargetHeight(OutputTarget target) const { return targetHeight[(int)target]; }

		ImageRenderer* getImageRenderer() override { return imageRenderer; }
		GlyphRenderer* getGlyphRenderer() override { return glyphRenderer; }
		EmulatorVideoDriver* getEmulatorVideo() override;

		//!False once the OS has taken away the foreground (HOME menu overlay,
		//!forced exit, etc.) - GX2 is off-limits at that point, so every
		//!draw/render entry point below checks this first and no-ops rather
		//!than issuing a GX2 call into a context we no longer own.
		bool isForeground() const;

		// pipelined == false: the original blocking present (used by the menu,
		//   where nothing is time-critical): copy, swap, flush, DrawDone,
		//   then wait for the flip before returning.
		// pipelined == true (used by the emulator, see WutEmulatorVideo::
		//   presentFrame): copy, swap, flush and return. The GPU renders and
		//   the flip happens at vblank while the CPU is already emulating
		//   the next frame; the wait for that flip is done at the *next*
		//   present, just before the next scan-buffer copy.
		void presentBuffer(bool pipelined = false);

		// Emulator present path only (see presentFrame). Blocks until the
		// GPU has retired the last submitted frame. Must be called before
		// the CPU touches anything the GPU reads (emulator texture,
		// vertex/uniform buffers, texture realloc). Normally a no-op: the
		// GPU finishes ~3 ms after submit, well before the next frame is
		// ready to present.
		void waitGpuRetired();

		// DrawDone + wait for outstanding flips. Called when leaving the
		// emulator for the menu, and before shutdown, so a still-in-flight
		// emulator frame can never be seen by code that assumes the
		// blocking (menu) present model.
		void drainGpu();
	private:
		// Binds the TV context state and resets the per-frame render
		// state (viewport/scissor/blend/depth/cull) that WHBGfxInit()
		// doesn't set on its own. Called once at the end of init() so
		// the first frame's draws land somewhere valid, then again at
		// the top of every render() pass.
		void prepareFrame(bool waitForFlip = true);

		bool gpuFramesInFlight = false;   // a pipelined frame was submitted and not yet drained
		OSTime lastSubmitTimeStamp = 0;   // GX2 timestamp of that submit

		// Queries GX2's current TV scan mode/aspect ratio and derives the
		// physical TV and DRC target dims
		void computeUIScale();

		int screenWidth;
		int screenHeight;
		float uiScale = 1.0f;
		int targetWidth[OUTPUT_TARGET_COUNT] = { 0, 0 };
		int targetHeight[OUTPUT_TARGET_COUNT] = { 0, 0 };
		PixelColor clearColor;

		ImageRenderer * imageRenderer;
		GlyphRenderer * glyphRenderer;
		WutEmulatorVideo* emulatorVideo = nullptr;
};

//!GX2-backed ImageRenderer for GuiImage/GuiImageData, using Texture2DShader.
class WutImageRenderer : public ImageRenderer
{
	public:
		WutImageRenderer(WutVideoDriver * driver);

		void * createTexture(int width, int height) override;
		void loadTextureData(void * texture, const uint8_t * rgba, int width, int height) override;
		void fillTexture(void * texture, int width, int height, PixelSourceFn source, void * userdata) override;
		void destroyTexture(void * texture) override;
		void drawTexture(void * texture, float xpos, float ypos, uint16_t width, uint16_t height, float degrees, float scaleX, float scaleY, uint8_t alpha) override;
		void drawRectangle(float x, float y, float width, float height, PixelColor color) override;

	private:
		WutVideoDriver * driver;
		GX2Sampler sampler;
};

//!GX2-backed GlyphRenderer for GuiTextRenderer, using Texture2DShader for
//!glyph quads and ColorShader for solid "feature" rectangles.
class WutGlyphRenderer : public GlyphRenderer
{
	public:
		WutGlyphRenderer(WutVideoDriver * driver);

		void* createTexture(uint16_t width, uint16_t height) override;
		void loadTextureData(void* texture, FT_Bitmap* bitmap) override;
		void destroyTexture(void* texture) override;

		void drawQuad(void* texture, int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color) override;
		void drawFeature(int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color) override;

	private:
		WutVideoDriver * driver;
		GX2Sampler sampler;
};

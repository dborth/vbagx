/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * WutEmulatorVideo.cpp
 ***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <algorithm>

#include <coreinit/memdefaultheap.h>
#include <coreinit/time.h>
#include <gx2/mem.h>
#include <whb/gfx.h>

#include "WutEmulatorVideo.h"
#include "WutVideoDriver.h"
#include "WutScaleFX.h"
#include "WutOutputFilter.h"
#include "WutUpscaleFilters.h"
#include "shaders/Texture2DShader.h"
#include "../../vbagx.h"
#include "../../vbasupport.h"
#include "../../gameborder.h"
#include "../../video.h"
#include "../../menu.h"
#include "../../vba/gba/Globals.h"
#include "../../vba/gba/Debug.h"

#include "fps_font_png.h"

namespace
{
	// Darkness of the scanline gaps (0..1) when Scanline Overlay is on
	const float SCANLINE_STRENGTH = 0.5f;

	// Emulator video is placed in the physical pixels of each output target.
	// The constants below are NOT a design canvas - they are the units the saved
	// settings and the shared HD art are defined in.

	// Units of the Screen Position (videoXshift/videoYshift) setting: one unit is
	// 1/640 of the screen width, 1/480 of the screen height. Same on every
	// platform, so a saved shift looks the same everywhere.
	const float SHIFT_UNITS_X = 640.0f;
	const float SHIFT_UNITS_Y = 480.0f;

	// Fixed Pixel Ratio: "Nx" is N output pixels per console pixel on a
	// 480-line screen (what GC/Wii give), scaled up by whole pixels for taller targets
	const float FIXED_REFERENCE_LINES = 480.0f;

	// Overlay art (FPS font, pointer) is authored for a 1080-line target and drawn
	// at its native size there, scaled uniformly by targetHeight / 1080 elsewhere
	const float HD_ASSET_LINES = 1080.0f;

	// FPS readout anchor, as a fraction of the target, and its glyph advance in HD art pixels
	const float FPS_ANCHOR_X = 0.75f;
	const float FPS_ANCHOR_Y = 0.875f;
	const float FPS_ADVANCE_HD = 42.0f;

	// FPS atlas layout: 16 equal-width cells across (0-9, '.', 'F', 'P', 'S', ':', blank).
	// Each cell's UV rect lives in its own immutable slot, padded to
	// GX2_VERTEX_BUFFER_ALIGNMENT so every slot's address is a legal
	// GX2SetAttribBuffer() pointer.
	const int      fpsGlyphCells      = 16;
	const uint32_t fpsGlyphUvSize     = 4 * Shader::cuTexCoordAttrSize;
	const uint32_t fpsGlyphSlotSize   = GX2_VERTEX_BUFFER_ALIGNMENT;
	const uint32_t fpsGlyphSlotFloats = fpsGlyphSlotSize / sizeof(float);

	// Screen tilt (Yoshi's Universal Gravitation): the game quad turns with the
	// Wiimote and is shrunk so its corners stay on screen - same factor as GC/Wii
	const float TILT_SCREEN_SCALE = 0.8f;

	// Corners (NDC) of a pixel rect turned `degrees` clockwise about its own
	// centre and scaled by `shrink`. Rotating in physical pixels keeps the
	// turn rigid on any target aspect ratio. Vertex order matches
	// Texture2DShader's unit quad: bottom-left, bottom-right, top-right, top-left.
	void TiltedQuadToNdc(float x, float y, float w, float h, float degrees, float shrink, int designWidth, int designHeight, float corners[8])
	{
		const float cx = x + w * 0.5f;
		const float cy = y + h * 0.5f;
		const float hw = w * shrink * 0.5f;
		const float hh = h * shrink * 0.5f;
		const float rad = degrees * ((float)M_PI / 180.0f);
		const float c = cosf(rad);
		const float s = sinf(rad);

		static const float lx[4] = { -1.0f,  1.0f, 1.0f, -1.0f };
		static const float ly[4] = { -1.0f, -1.0f, 1.0f,  1.0f }; // +1 = top of the quad

		for (int i = 0; i < 4; i++)
		{
			const float dx = lx[i] * hw;
			const float dy = -ly[i] * hh; // pixel space is y-down
			const float px = cx + dx * c - dy * s;
			const float py = cy + dx * s + dy * c;

			corners[i * 2 + 0] = (px / designWidth) * 2.0f - 1.0f;
			corners[i * 2 + 1] = 1.0f - (py / designHeight) * 2.0f;
		}
	}

	void PixelRectToNdc(float x, float y, float w, float h, int designWidth, int designHeight, float offset[3], float scale[3])
	{
		float centerPxX = x + w * 0.5f;
		float centerPxY = y + h * 0.5f;

		offset[0] = (centerPxX / designWidth) * 2.0f - 1.0f;
		offset[1] = 1.0f - (centerPxY / designHeight) * 2.0f;
		offset[2] = 0.0f;

		scale[0] = w / designWidth;
		scale[1] = h / designHeight;
		scale[2] = 1.0f;
	}
}

WutEmulatorVideo::WutEmulatorVideo()
	: videoDriver(nullptr), texture(nullptr)
	, vwidth(0), vheight(0), oldvwidth(0), oldvheight(0)
	, checkVideo(1)
	, quadX(0), quadY(0), quadWidth(0), quadHeight(0)
	, placement{ {0, 0, 0, 0}, {0, 0, 0, 0} }
	, fpsFont(nullptr), fpsGlyphTexCoords(nullptr), lastFpsTime(0)
	, screenshotSnapshot(nullptr), screenshotWidth(0), screenshotHeight(0), screenshotPitch(0)
{
	fpsStr[0] = '\0';

	GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
	GX2InitSampler(&fontSampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);
	GX2InitSampler(&uiSampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
}

WutEmulatorVideo::~WutEmulatorVideo()
{
	destroyTexture();
	if(fpsFont) {
		delete fpsFont;
		fpsFont = nullptr;
	}
	if(fpsGlyphTexCoords) {
		free(fpsGlyphTexCoords);
		fpsGlyphTexCoords = nullptr;
	}
	if(screenshotSnapshot) {
		free(screenshotSnapshot);
		screenshotSnapshot = nullptr;
	}
}

void WutEmulatorVideo::init(VideoDriver* driver)
{
	videoDriver = static_cast<WutVideoDriver*>(driver);
}

/****************************************************************************
 * resetVideo
 *
 * Computes where the game quad goes on each output target, in that target's
 * own physical pixels (placement[]), from the current vwidth/vheight and
 * EmuSettings' aspect ratio / zoom / fixed-scale / shift options.
 *
 * Nothing here depends on the UI canvas. Every target's buffer is
 * square-pixel (640x480, 854x480, 1280x720, 1920x1080 TV modes, and the
 * 854x480 GamePad), so a target's aspect ratio is simply width/height and
 * there is no anamorphic compensation - the Wii's "(4/3)/tv" term exists
 * because its EFB is always 640x480 whatever the TV shape. TV and GamePad are
 * fitted independently, so a 4:3 TV and the 16:9 GamePad each get a correctly
 * shaped picture.
 *
 * The console picture is stretched up to maxStretchRatio (1.0 maintain, 
 * 1.3 partial, 1.6 stretch to fit) towards the target's aspect ratio, then 
 * letterboxed/pillarboxed if that falls short.
 *
 * quadX/Y/Width/Height (and gameScreenPng) are the TV placement expressed in
 * UI-canvas pixels. They only exist for the menu's game screenshot, which
 * lives in canvas space; nothing is drawn from them.
 ***************************************************************************/
void WutEmulatorVideo::resetVideo()
{
	if (vwidth <= 0 || vheight <= 0)
		return;

	const float maxStretchRatio =
		(EmuSettings.videoAspectRatioCorrection == SCALING_PARTIAL_STRETCH) ? 1.3f :
		(EmuSettings.videoAspectRatioCorrection == SCALING_STRETCH_TO_FIT)  ? 1.6f : 1.0f;

	const float consoleAspectRatio = (float)vwidth / (float)vheight;

	float zoomHor, zoomVert;
	int fixed;
	if (cartridgeType == CARTRIDGE_GBA)
	{
		zoomHor  = EmuSettings.gbaZoomHor;
		zoomVert = EmuSettings.gbaZoomVert;
		fixed    = EmuSettings.gbaFixed;
	}
	else
	{
		zoomHor  = EmuSettings.gbZoomHor;
		zoomVert = EmuSettings.gbZoomVert;
		fixed    = EmuSettings.gbFixed;
	}

	for (int i = 0; i < OUTPUT_TARGET_COUNT; i++)
	{
		const OutputTarget target = static_cast<OutputTarget>(i);
		const float targetW = (float)videoDriver->getTargetWidth(target);
		const float targetH = (float)videoDriver->getTargetHeight(target);

		float w, h;
		if (fixed)
		{
			// Whole output pixels per console pixel, square. The "16:9 correction"
			// bit of 'fixed' is an anamorphic-output workaround and never applies here.
			const int ratio = fixed % 10;
			const float pixelScale = std::max(1.0f, floorf(ratio * targetH / FIXED_REFERENCE_LINES + 0.001f));
			w = (float)vwidth  * pixelScale;
			h = (float)vheight * pixelScale;
		}
		else
		{
			const float targetAspectRatio = targetW / targetH;

			float fillW, fillH; // fraction of the target
			if (targetAspectRatio > consoleAspectRatio)
			{
				fillH = 1.0f;
				float stretchRatio = std::min(targetAspectRatio / consoleAspectRatio, maxStretchRatio);
				fillW = consoleAspectRatio * stretchRatio / targetAspectRatio;
			}
			else
			{
				fillW = 1.0f;
				float stretchRatio = std::min(consoleAspectRatio / targetAspectRatio, maxStretchRatio);
				fillH = targetAspectRatio * stretchRatio / consoleAspectRatio;
			}

			w = targetW * fillW * zoomHor;
			h = targetH * fillH * zoomVert;
		}

		float x = (targetW - w) * 0.5f + EmuSettings.videoXshift * (targetW / SHIFT_UNITS_X);
		float y = (targetH - h) * 0.5f + EmuSettings.videoYshift * (targetH / SHIFT_UNITS_Y);

		if (fixed)
		{
			// Pixel-exact only if the quad also starts on a pixel boundary
			x = floorf(x + 0.5f);
			y = floorf(y + 0.5f);
		}

		placement[i].x = x;
		placement[i].y = y;
		placement[i].w = w;
		placement[i].h = h;
	}

	// The same placement in UI-canvas pixels (menu screenshot), from the TV's placement
	const TargetPlacement& tv = placement[static_cast<int>(OutputTarget::TV)];
	const float toCanvasX = (float)videoDriver->getScreenWidth()  / (float)videoDriver->getTargetWidth(OutputTarget::TV);
	const float toCanvasY = (float)videoDriver->getScreenHeight() / (float)videoDriver->getTargetHeight(OutputTarget::TV);

	quadX      = tv.x * toCanvasX;
	quadY      = tv.y * toCanvasY;
	quadWidth  = tv.w * toCanvasX;
	quadHeight = tv.h * toCanvasY;

	gameScreenPng.width  = vwidth;
	gameScreenPng.height = vheight;
	gameScreenPng.scaleX = quadWidth  / (float)vwidth;
	gameScreenPng.scaleY = quadHeight / (float)vheight;
	gameScreenPng.xoffset = (quadX + quadWidth  * 0.5f) - (videoDriver->getScreenWidth()  * 0.5f);
	gameScreenPng.yoffset = (quadY + quadHeight * 0.5f) - (videoDriver->getScreenHeight() * 0.5f);
}

/****************************************************************************
 * mapPointerToUnit
 *
 * The pointer is reported in UI-canvas coordinates, which span the whole
 * screen on every output, so they are a fraction of the target the pointer is
 * on. That is mapped through that target's own placement (the TV and the
 * GamePad are fitted independently, so they differ).
 ***************************************************************************/
bool WutEmulatorVideo::mapPointerToUnit(float canvasX, float canvasY, bool onGamePad, float* u, float* v)
{
	if (!u || !v)
		return false;

	const OutputTarget target = onGamePad ? OutputTarget::DRC : OutputTarget::TV;
	const TargetPlacement& p = placement[static_cast<int>(target)];
	if (p.w <= 0.0f || p.h <= 0.0f) // resetVideo() hasn't run yet
		return false;

	const float px = (canvasX / (float)videoDriver->getScreenWidth())  * (float)videoDriver->getTargetWidth(target);
	const float py = (canvasY / (float)videoDriver->getScreenHeight()) * (float)videoDriver->getTargetHeight(target);

	const float fx = (px - p.x) / p.w;
	const float fy = (py - p.y) / p.h;
	*u = fx < 0.0f ? 0.0f : (fx > 1.0f ? 1.0f : fx);
	*v = fy < 0.0f ? 0.0f : (fy > 1.0f ? 1.0f : fy);
	return true;
}

/****************************************************************************
 * rebuildTexture / destroyTexture
 *
 * The game texture is recreated only when the emulator's rendered
 * width/height actually changes (see presentFrame), not every frame.
 ***************************************************************************/
void WutEmulatorVideo::destroyTexture()
{
	if (!texture)
		return;

	if (texture->surface.image)
		MEMFreeToDefaultHeap(texture->surface.image);

	delete texture;
	texture = nullptr;
}

void WutEmulatorVideo::rebuildTexture(int width, int height)
{
	destroyTexture();

	if (width <= 0 || height <= 0)
		return;

	texture = new GX2Texture();
	GX2InitTexture(texture, width, height, 1, 0, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_SURFACE_DIM_TEXTURE_2D, GX2_TILE_MODE_LINEAR_ALIGNED);

	GX2CalcSurfaceSizeAndAlignment(&texture->surface);
	GX2InitTextureRegs(texture);

	texture->surface.image = MEMAllocFromDefaultHeapEx(texture->surface.imageSize, texture->surface.alignment);
	if (!texture->surface.image)
	{
		delete texture;
		texture = nullptr;
	}
}

/****************************************************************************
 * uploadFrame
 *
 * Converts the console's raw RGB555 framebuffer into an RGBA8 GX2 texture.
 *
 * gbWidth/gbHeight are the dimensions of the incoming frame actually
 * sitting in 'pix' this call (the raw parameter presentFrame() received),
 * which is *not* necessarily the same as vwidth/vheight: when gameBorder
 * holds a border, vwidth/vheight describe the larger bordered canvas the 
 * quad/texture are sized to, and the console's own frame is composited 
 * centered inside it.
 ***************************************************************************/
void WutEmulatorVideo::uploadFrame(int gbWidth, int gbHeight)
{
	if (!texture || !texture->surface.image || gbWidth <= 0 || gbHeight <= 0)
		return;

	const uint8_t* buffer = pix;
	if (cartridgeType == CARTRIDGE_GBA)
		buffer += 484; // skip the uninitialized top row

	uint8_t* dst = static_cast<uint8_t*>(texture->surface.image);
	const uint32_t dstStride = texture->surface.pitch * 4; // bytes/row, RGBA8

	int offsetX = 0;
	int offsetY = 0;

	bool useBorder = gameBorder.hasBorder();
	if (useBorder)
	{
		// One-time (or whenever dirty) sync of the border's own RGBA8
		// pixels into the texture, before the console frame is blitted
		// on top of the middle of it.
		if (gameBorder.needsSync())
		{
			const uint8_t* borderRgba = gameBorder.getPixelsRGBA8();
			int bw = gameBorder.getWidth();
			int bh = gameBorder.getHeight();
			for (int y = 0; y < bh; y++)
				memcpy(dst + y * dstStride, borderRgba + y * bw * 4, bw * 4);
			gameBorder.markSynced();
		}

		offsetX = (gameBorder.getWidth()  - gbWidth)  / 2;
		offsetY = (gameBorder.getHeight() - gbHeight) / 2;
	}

	// VBA-M core pitch: 2 bytes/pixel plus a 4-byte pad
	const int gbPitch = gbWidth * 2 + 4;

	for (int y = 0; y < gbHeight; y++)
	{
		const uint16_t* srcRow = reinterpret_cast<const uint16_t*>(buffer + y * gbPitch);
		uint8_t* dstRow = dst + (offsetY + y) * dstStride + offsetX * 4;

		for (int x = 0; x < gbWidth; x++)
		{
			uint16_t px = srcRow[x];

			// RGB555 (bit 15 unused/opaque marker)
			uint8_t r5 = (px >> 10) & 0x1F;
			uint8_t g5 = (px >> 5)  & 0x1F;
			uint8_t b5 =  px        & 0x1F;

			uint8_t* out = dstRow + x * 4;
			out[0] = (r5 << 3) | (r5 >> 2);
			out[1] = (g5 << 3) | (g5 >> 2);
			out[2] = (b5 << 3) | (b5 >> 2);
			out[3] = 0xFF;
		}
	}

	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, texture->surface.image, texture->surface.imageSize);
}

/****************************************************************************
 * drawQuad
 *
 * Draws the game frame - plain, sharp-bilinear/scanline filtered, or
 * ScaleFX-upscaled (TV only) - sized/positioned per render target via
 * placement[]/placementNdc, into whatever render target
 * WHBGfxBeginRenderTV()/BeginRenderDRC() is currently bound to.
 ***************************************************************************/
void WutEmulatorVideo::drawQuad()
{
	videoDriver->flushDrawQueue();

	if (!texture || !videoDriver->isForeground())
		return;

	// Cheap enough to just re-set every frame rather than tracking whether
	// the setting changed since the last draw.
	GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_CLAMP,
		EmuSettings.videoBilinearFilter ? GX2_TEX_XY_FILTER_MODE_LINEAR : GX2_TEX_XY_FILTER_MODE_POINT);

	float colorIntensity[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	Texture2DShader* shader = Texture2DShader::instance();

	// NDC placement of the game quad on a target, from its physical-pixel rect
	auto placementNdc = [&](OutputTarget target, float offset[3], float scale[3]) {
		const TargetPlacement& p = placement[static_cast<int>(target)];
		PixelRectToNdc(p.x, p.y, p.w, p.h, videoDriver->getTargetWidth(target), videoDriver->getTargetHeight(target), offset, scale);
	};

	auto drawPass = [&](OutputTarget target) {
		float offset[3];
		float scale[3];
		placementNdc(target, offset, scale);

		shader->setShaders();
		shader->setAttributeBuffer();
		shader->setAngle(0.0f);
		shader->setOffset(offset);
		shader->setScale(scale);
		shader->setColorIntensity(colorIntensity);
		shader->clearBlur();
		shader->setTextureAndSampler(texture, &sampler);
		shader->draw(GX2_PRIMITIVE_MODE_QUADS, 4);
	};

	// Tilted game quad: pre-rotated corners, drawn plain (the output filters and
	// ScaleFX are skipped while the screen is tilting). Returns false if the
	// per-frame rotated-quad slots are exhausted - the caller draws it straight.
	auto drawTiltedPass = [&](OutputTarget target) {
		const TargetPlacement& p = placement[static_cast<int>(target)];

		float corners[8];
		TiltedQuadToNdc(p.x, p.y, p.w, p.h, TiltAngle, TILT_SCREEN_SCALE,
			videoDriver->getTargetWidth(target), videoDriver->getTargetHeight(target), corners);

		uint32_t slot = 0;
		if (!shader->uploadRotatedQuad(corners, slot))
			return false;

		static const float identityOffset[3] = { 0.0f, 0.0f, 0.0f };
		static const float identityScale[3] = { 1.0f, 1.0f, 1.0f };

		shader->setShaders();
		shader->setRotatedAttributeBuffer(slot);
		shader->setAngle(0.0f);
		shader->setOffset(identityOffset);
		shader->setScale(identityScale);
		shader->setColorIntensity(colorIntensity);
		shader->clearBlur();
		shader->setTextureAndSampler(texture, &sampler);
		shader->draw(GX2_PRIMITIVE_MODE_QUADS, 4);
		return true;
	};

	const bool sharp = EmuSettings.videoUpscalingFilter == UPSCALE_SHARP_BILINEAR;
	const float scanlines = EmuSettings.videoScanlines ? SCANLINE_STRENGTH : 0.0f;

	// Output filter: sharp bilinear and/or scanlines. Returns false if it is unavailable.
	auto outputFilterPass = [&](OutputTarget target, const GX2Texture* tex, bool linear, bool sharpSampling) {
		const TargetPlacement& p = placement[static_cast<int>(target)];

		WutOutputFilter::Params pp;
		pp.texture = tex;
		placementNdc(target, pp.offset, pp.scale);
		pp.outWidth = p.w;
		pp.outHeight = p.h;
		pp.linear = linear;
		pp.sharp = sharpSampling;
		pp.scanlineStrength = scanlines;
		pp.sourceLines = (float) texture->surface.height;
		return WutOutputFilter::instance()->draw(pp);
	};

	// The frame texture on a target: plain textured quad, or the output filter when it has work to do
	auto drawGame = [&](OutputTarget target) {
		if (TiltScreen && drawTiltedPass(target))
			return;
		if ((sharp || scanlines > 0.0f) && outputFilterPass(target, texture, EmuSettings.videoBilinearFilter, sharp))
			return;
		drawPass(target);
	};

	// ScaleFX (TV output only)
	WutScaleFX* scalefx = WutScaleFX::instance();
	bool useScaleFX = false;

	if (EmuSettings.videoUpscalingFilter == UPSCALE_SCALEFX)
	{
		if (!TiltScreen && scalefx->prepare(texture->surface.width, texture->surface.height))
		{
			scalefx->run(texture);
			useScaleFX = true;
		}
	}
	else
	{
		scalefx->release();
	}

	WHBGfxBeginRenderTV();
	if (useScaleFX)
	{
		// Scanlines go through the output filter, otherwise the ScaleFX final stage draws it
		if (scanlines <= 0.0f || !outputFilterPass(OutputTarget::TV, scalefx->outputTexture(), true, false))
		{
			float offset[3];
			float scale[3];
			placementNdc(OutputTarget::TV, offset, scale);
			scalefx->drawTV(offset, scale);
		}
	}
	else
	{
		drawGame(OutputTarget::TV);
	}
	WHBGfxBeginRenderDRC(); drawGame(OutputTarget::DRC);
}

/****************************************************************************
 * drawFpsOverlay
 ***************************************************************************/
void WutEmulatorVideo::drawFpsOverlay()
{
	videoDriver->flushDrawQueue(); // overlays draw directly; keep queued UI draws underneath them
	if (!EmuSettings.displayFrameRate || !videoDriver->isForeground())
		return;

	if (!fpsFont || !fpsGlyphTexCoords)
		return;

	GX2Texture* fontTex = static_cast<GX2Texture*>(fpsFont->getTexture());
	if (!fontTex)
		return;

	uint32_t nowMs = (uint32_t)OSTicksToMilliseconds(OSGetTime());
	if (fpsStr[0] == '\0' || nowMs - lastFpsTime >= 1000)
	{
		float fps = (EmuSettings.displayFrameRate == FRAMERATE_CORE) ? systemGetCoreFPS() : systemGetRenderFPS();
		snprintf(fpsStr, sizeof(fpsStr), "FPS: %.1f", fps);
		lastFpsTime = nowMs;
	}

	// Native size of the (HD) glyph cell, in HD art pixels
	const float glyphW = (float)fontTex->surface.width / (float)fpsGlyphCells;
	const float glyphH = (float)fontTex->surface.height;

	Texture2DShader* shader = Texture2DShader::instance();
	float colorIntensity[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	float penHd = 0.0f; // advance so far, in HD art pixels
	for (int i = 0; fpsStr[i] != '\0'; i++)
	{
		char c = fpsStr[i];
		int texIdx = 15; // default: blank cell
		if (c >= '0' && c <= '9') texIdx = c - '0';
		else if (c == '.') texIdx = 10;
		else if (c == 'F') texIdx = 11;
		else if (c == 'P') texIdx = 12;
		else if (c == 'S') texIdx = 13;
		else if (c == ':') texIdx = 14;

		// Each glyph has its own pre-baked UV slot. GX2 draws are asynchronous, so any CPU write
		// made after this draw is recorded would be seen by it.
		const float* glyphUvs = fpsGlyphTexCoords + texIdx * fpsGlyphSlotFloats;

		auto drawPass = [&](OutputTarget target) {
			const float targetW = (float)videoDriver->getTargetWidth(target);
			const float targetH = (float)videoDriver->getTargetHeight(target);
			const float s = targetH / HD_ASSET_LINES; // uniform: art keeps its shape on any target

			float offset[3], scale[3];
			PixelRectToNdc(targetW * FPS_ANCHOR_X + penHd * s, targetH * FPS_ANCHOR_Y, glyphW * s, glyphH * s,
				(int)targetW, (int)targetH, offset, scale);

			shader->setShaders();
			shader->setAttributeBuffer();
			VertexShader::setAttributeBuffer(1, fpsGlyphUvSize, Shader::cuTexCoordAttrSize, glyphUvs);
			shader->setAngle(0.0f);
			shader->setOffset(offset);
			shader->setScale(scale);
			shader->setColorIntensity(colorIntensity);
			shader->clearBlur();
			shader->setTextureAndSampler(fontTex, &fontSampler);
			shader->draw(GX2_PRIMITIVE_MODE_QUADS, 4);
		};

		WHBGfxBeginRenderTV(); drawPass(OutputTarget::TV);
		WHBGfxBeginRenderDRC(); drawPass(OutputTarget::DRC);

		penHd += FPS_ADVANCE_HD;
	}
}

/****************************************************************************
 * drawCursorOverlay
 ***************************************************************************/
void WutEmulatorVideo::drawCursorOverlay()
{
	videoDriver->flushDrawQueue(); // overlays draw directly; keep queued UI draws underneath them
	if (!CursorVisible || !CursorValid || !videoDriver->isForeground())
		return;

	GuiImageData* cursorImg = pointer[0];
	if (!cursorImg)
		return;

	GX2Texture* cursorTex = static_cast<GX2Texture*>(cursorImg->getTexture());
	if (!cursorTex)
		return;

	// Position comes from the input driver in UI-canvas coordinates, so it is taken
	// as a fraction of the screen; the size is the HD art's own, in physical pixels
	const float u = (float)CursorX / (float)videoDriver->getScreenWidth();
	const float v = (float)CursorY / (float)videoDriver->getScreenHeight();

	float colorIntensity[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	Texture2DShader* shader = Texture2DShader::instance();

	auto drawPass = [&](OutputTarget target) {
		const float targetW = (float)videoDriver->getTargetWidth(target);
		const float targetH = (float)videoDriver->getTargetHeight(target);
		const float s = targetH / HD_ASSET_LINES; // uniform: art keeps its shape on any target
		const float w = (float)cursorTex->surface.width  * s;
		const float h = (float)cursorTex->surface.height * s;

		float offset[3], scale[3];
		PixelRectToNdc(u * targetW - w * 0.5f, v * targetH - h * 0.5f, w, h, (int)targetW, (int)targetH, offset, scale);

		shader->setShaders();
		shader->setAttributeBuffer();
		shader->setAngle(0.0f);
		shader->setOffset(offset);
		shader->setScale(scale);
		shader->setColorIntensity(colorIntensity);
		shader->clearBlur();
		shader->setTextureAndSampler(cursorTex, &uiSampler);
		shader->draw(GX2_PRIMITIVE_MODE_QUADS, 4);
	};

	WHBGfxBeginRenderTV(); drawPass(OutputTarget::TV);
	WHBGfxBeginRenderDRC(); drawPass(OutputTarget::DRC);
}

/****************************************************************************
 * presentFrame
 *
 * width/height are the raw console frame dimensions for *this* call
 * When gameBorder holds a border, the target is sized to the border instead
 ***************************************************************************/
void WutEmulatorVideo::presentFrame(int width, int height)
{
	videoDriver->prepareFrame();

	bool useBorder = gameBorder.hasBorder();
	vwidth  = useBorder ? gameBorder.getWidth()  : width;
	vheight = useBorder ? gameBorder.getHeight() : height;

	if (checkVideo || vwidth != oldvwidth || vheight != oldvheight)
	{
		resetVideo(); // recompute quad placement for the (possibly new) target size
		rebuildTexture(vwidth, vheight);

		oldvwidth = vwidth;
		oldvheight = vheight;
		checkVideo = 0;
	}

	uploadFrame(width, height);
	drawQuad();
	drawFpsOverlay();
	drawCursorOverlay();

	videoDriver->presentBuffer();
}

/****************************************************************************
 * snapshotFrame / readFrameRGB24
 ***************************************************************************/
void WutEmulatorVideo::snapshotFrame()
{
	if (screenshotSnapshot)
	{
		free(screenshotSnapshot);
		screenshotSnapshot = nullptr;
	}

	if (!texture || !texture->surface.image)
		return;

	screenshotSnapshot = (uint8_t*)malloc(texture->surface.imageSize);
	if (!screenshotSnapshot)
		return;

	memcpy(screenshotSnapshot, texture->surface.image, texture->surface.imageSize);
	screenshotWidth  = vwidth;
	screenshotHeight = vheight;
	screenshotPitch  = texture->surface.pitch;
}

void WutEmulatorVideo::readFrameRGB24(int width, int height, uint8_t* dst)
{
	if (!screenshotSnapshot || !dst)
		return;

	// width/height come from gameScreenPng
	if (width != screenshotWidth || height != screenshotHeight)
	{
		free(screenshotSnapshot);
		screenshotSnapshot = nullptr;
		return;
	}

	const uint32_t srcStride = screenshotPitch * 4; // bytes/row, RGBA8

	for (int y = 0; y < height; y++)
	{
		const uint8_t* srcRow = screenshotSnapshot + y * srcStride;
		uint8_t* dstRow = dst + y * width * 3;

		for (int x = 0; x < width; x++)
		{
			dstRow[x * 3 + 0] = srcRow[x * 4 + 0];
			dstRow[x * 3 + 1] = srcRow[x * 4 + 1];
			dstRow[x * 3 + 2] = srcRow[x * 4 + 2];
		}
	}

	free(screenshotSnapshot);
	screenshotSnapshot = nullptr;
}

/****************************************************************************
 * renderInit
 *
 * Sets the initial (possibly border-inclusive) target dimensions, ahead of
 * the first presentFrame() call, and forces the next presentFrame() to
 * rebuild the texture and re-run resetVideo() regardless of whether the
 * size happens to match whatever was already there.
 ***************************************************************************/
void WutEmulatorVideo::renderInit(int width, int height)
{
	vwidth = width;
	vheight = height;
	checkVideo = 1;
}

/****************************************************************************
 * initFPSFontData
 *
 * Called once at startup (before any memory-mode switching happens).
 ***************************************************************************/
void WutEmulatorVideo::initFPSFontData()
{
	if (!fpsFont)
		fpsFont = new GuiImageData(fps_font_png);

	if (!fpsGlyphTexCoords)
	{
		const uint32_t totalSize = fpsGlyphCells * fpsGlyphSlotSize;
		fpsGlyphTexCoords = static_cast<float*>(memalign(GX2_VERTEX_BUFFER_ALIGNMENT, totalSize));
		if (!fpsGlyphTexCoords)
			return;

		memset(fpsGlyphTexCoords, 0, totalSize);

		// Cells are equal-width across the atlas, so cell i spans [i/16, (i+1)/16]
		// in U regardless of the atlas' pixel width. Same vertex order and V flip
		// as the default unit quad (BL, BR, TR, TL).
		for (int i = 0; i < fpsGlyphCells; i++)
		{
			float* uv = fpsGlyphTexCoords + i * fpsGlyphSlotFloats;
			const float u0 = (float)i / (float)fpsGlyphCells;
			const float u1 = (float)(i + 1) / (float)fpsGlyphCells;

			uv[0] = u0; uv[1] = 1.0f;
			uv[2] = u1; uv[3] = 1.0f;
			uv[4] = u1; uv[5] = 0.0f;
			uv[6] = u0; uv[7] = 0.0f;
		}

		// Written once, read-only from here on
		GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, fpsGlyphTexCoords, totalSize);
	}
}

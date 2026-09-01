/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * gameborder.cpp
 *
 ***************************************************************************/

#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "vbagx.h"
#include "gameborder.h"
#include "vbasupport.h"
#include "memmanager.h"
#include "fileop.h"
#include "utils/pngcodec.h"
#include "goomba/goombarom.h"
#include "vba/gba/Globals.h"
#include "vba/gb/gbGlobals.h"

SgbBorderExtractor sgbBorderExtractor;
GameBorder gameBorder;

SgbBorderExtractor::SgbBorderExtractor() :
		isActive(false), scanThrottle(0) {
}

void SgbBorderExtractor::reset(bool isSgbGame, bool borderAlreadyLoaded) {
	isActive = (isSgbGame && !borderAlreadyLoaded);
	scanThrottle = 0;
}

// True if every pixel outside the centered GB screen still matches
// buffer[0], i.e. the game hasn't painted a border yet.
static bool rowIsUniform(const u16* buffer, int y, int xStart, int xEnd, u16 reference) {
	for (int x = xStart; x < xEnd; x++) {
		if (buffer[SGB_FRAME_WIDTH * y + x] != reference)
			return false;
	}
	return true;
}

bool SgbBorderExtractor::isBorderAreaEmpty(const u16 *buffer) {
	u16 reference = buffer[0];

	// Top / bottom borders (full width strips)
	for (int y = 0; y < SGB_BORDER_TOP; y++)
		if (!rowIsUniform(buffer, y, 0, SGB_FRAME_WIDTH, reference))
			return false;
	for (int y = SGB_FRAME_HEIGHT - SGB_BORDER_BOTTOM; y < SGB_FRAME_HEIGHT; y++)
		if (!rowIsUniform(buffer, y, 0, SGB_FRAME_WIDTH, reference))
			return false;

	// Left / right pillars flanking the GB screen
	for (int y = SGB_BORDER_TOP; y < SGB_BORDER_TOP + SGB_SCREEN_HEIGHT; y++) {
		if (!rowIsUniform(buffer, y, 0, SGB_BORDER_LEFT, reference))
			return false;
		if (!rowIsUniform(buffer, y, SGB_FRAME_WIDTH - SGB_BORDER_RIGHT, SGB_FRAME_WIDTH, reference))
			return false;
	}
	return true;
}

bool SgbBorderExtractor::processFrame(const u16 *buffer, int gbWidth, int gbHeight) {
	if (!isActive || gbWidth != SGB_FRAME_WIDTH || gbHeight != SGB_FRAME_HEIGHT)
		return false;

	scanThrottle++;
	if (scanThrottle >= 60) {
		scanThrottle = 0;
		if (!isBorderAreaEmpty(buffer)) {
			BorderManager::save(buffer);
			isActive = false; // Permanently disable for session
			return true;
		}
	}
	return false;
}

char * BorderManager::getPNGBorderPath(const char* title) {
	const char* method = pathPrefix[GCSettings.LoadMethod];
	const char* folder = GCSettings.BorderFolder;

	char title_buffer[16] = {0};

	if(title) {
		strncpy(title_buffer, title, 15);
	}
	else {
		// If no title was passed in, get the rom title
		if (cartridgeType == CARTRIDGE_GB) {
			gb_get_title(gbRom, title_buffer);
		} else if (cartridgeType == CARTRIDGE_GBA) {
			memcpy(title_buffer, rom + 0xA0, 12);
			title_buffer[12] = '\0';
		}
	}

	size_t length = strlen(method) + strlen(folder) + strlen(title_buffer) + 6;
	char* path = (char*)mem1_malloc(length);
	if (path) sprintf(path, "%s%s/%s.png", method, folder, title_buffer);
	return path;
}

// Converts flat, row-major RGBA8 pixels into a newly allocated 4x4-tiled
// GX_TF_RGB5A3 buffer, opaque/RGB555-mode (bit15 set)
static u16 * TileRGBA8ToRGB555(const u8 *rgba, int width, int height)
{
	int padWidth = (width + 3) & ~3;
	int padHeight = (height + 3) & ~3;

	u16 *tiled = (u16 *) malloc(padWidth * padHeight * 2);
	if (!tiled)
		return nullptr;

	for (int y = 0; y < padHeight; y++) {
		int tile_y = y / 4;
		int in_tile_y = y % 4;
		for (int x = 0; x < padWidth; x++) {
			int tile_x = x / 4;
			int in_tile_x = x % 4;
			int idx = (tile_y * (padWidth / 4) + tile_x) * 16 + (in_tile_y * 4 + in_tile_x);

			u16 color = 0x8000; // RGB555 mode, opaque
			if (x < width && y < height) {
				const u8 *px = rgba + (y * width + x) * 4;
				u8 r5 = px[0] >> 3;
				u8 g5 = px[1] >> 3;
				u8 b5 = px[2] >> 3;
				color |= (r5 << 10) | (g5 << 5) | b5;
			}
			tiled[idx] = color;
		}
	}

	return tiled;
}

u16* BorderManager::load(const char *title, const char *fallback, int &outWidth, int &outHeight) {
	void *png_tmp_buf = mem1_malloc(1024 * 1024);
	char *borderPath = getPNGBorderPath(title);
	int imgWidth = 0, imgHeight = 0;
	u8 *rgba = nullptr;
	u16 *newBorder = nullptr;

	bool borderLoaded = LoadFile((char*) png_tmp_buf, borderPath, 0, 1024 * 1024, SILENT);
	if (!borderLoaded && fallback) {
		if (borderPath)
			mem1_free(borderPath);
		borderPath = getPNGBorderPath(fallback);
		borderLoaded = LoadFile((char*) png_tmp_buf, borderPath, 0, 1024 * 1024, SILENT);
	}
	if (!borderLoaded)
		goto cleanup;

	if (!PNGGetImageSize((const u8*) png_tmp_buf, &imgWidth, &imgHeight))
		goto cleanup;
	if (imgWidth > 640 || imgHeight > 480)
		goto cleanup;

	rgba = DecodePNGToRGBA8((const u8*) png_tmp_buf, imgWidth, imgHeight);
	if (!rgba)
		goto cleanup;

	newBorder = TileRGBA8ToRGB555(rgba, imgWidth, imgHeight);
	if (!newBorder)
		goto cleanup;

	outWidth = imgWidth;
	outHeight = imgHeight;

cleanup:
	if (rgba)
		mem1_free(rgba);
	if (png_tmp_buf)
		mem1_free(png_tmp_buf);
	if (borderPath)
		mem1_free(borderPath);

	return newBorder;
}

void BorderManager::save(const void* buffer) {
	char* borderPath = nullptr;
	FILE* f = nullptr;
	u8* rgb24 = nullptr;
	u8* png = nullptr;
	uint32_t pngSize = 0;

	int err;

	struct stat s;
	borderPath = getPNGBorderPath(nullptr);

	char* slash = strrchr(borderPath, '/');
	*slash = '\0'; // cut string off at directory name

	err = stat(borderPath, &s);
	if (err == -1) goto cleanup;
	if (!S_ISDIR(s.st_mode)) goto cleanup;

	*slash = '/'; // restore slash, bring filename back

	err = stat(borderPath, &s);
	if (err != -1 || errno != ENOENT) goto cleanup;

	f = fopen(borderPath, "wb");
	if (!f) goto cleanup;

	rgb24 = (u8*) mem1_malloc(SGB_FRAME_WIDTH * SGB_FRAME_HEIGHT * 3);
	if (!rgb24) goto cleanup;

	// buffer is the raw, linear (not GX-tiled) SGB framebuffer capture,
	// RGB555 pixels with a 258-pixel row stride - just convert to RGB24
	{
		const u16* src = (const u16*) buffer;
		for (int y = 0; y < SGB_FRAME_HEIGHT; y++) {
			const u16* srcRow = src + y * 258;
			u8* dstRow = rgb24 + y * SGB_FRAME_WIDTH * 3;
			for (int x = 0; x < SGB_FRAME_WIDTH; x++) {
				u16 color = srcRow[x];
				dstRow[x * 3]     = ((color >> 10) & 0x1F) << 3;
				dstRow[x * 3 + 1] = ((color >> 5) & 0x1F) << 3;
				dstRow[x * 3 + 2] = (color & 0x1F) << 3;
			}
		}
	}

	png = EncodePNGFromRGB24(SGB_FRAME_WIDTH, SGB_FRAME_HEIGHT, rgb24, 0, &pngSize);
	if (!png) goto cleanup;

	fwrite(png, 1, pngSize, f);

	cleanup:
	if (borderPath) mem1_free(borderPath);
	if (f) fclose(f);
	if (rgb24) mem1_free(rgb24);
	if (png) mem1_free(png);
}

GameBorder::GameBorder() :
	pixels(nullptr), width(0), height(0), needsTextureSync(false) {
}

GameBorder::~GameBorder() {
	clear();
}

void GameBorder::clear() {
	if (pixels) {
		free(pixels);
		pixels = nullptr;
	}
	width = 0;
	height = 0;
	needsTextureSync = false;
}

void GameBorder::setBorder(u16 *newPixels, int newWidth, int newHeight) {
	clear();
	if (newPixels) {
		pixels = newPixels;
		width = newWidth;
		height = newHeight;
		needsTextureSync = true;
	}
}

bool GameBorder::hasBorder() const {
	return pixels != nullptr;
}

void* GameBorder::applyToTexture(void *textureBase, int gbWidth, int gbHeight) {
	if (!pixels) {
		return textureBase; // Borderless fallback
	}

	// One-time GX texture sync if the border just changed
	if (needsTextureSync) {
		memcpy(textureBase, pixels, width * height * 2);
		DCStoreRange(textureBase, width * height * 2);
		needsTextureSync = false;
	}

	// Calculate exact center offset for the game viewport
	int offsetX = (width - gbWidth) / 2;
	int offsetY = (height - gbHeight) / 2;

	// Align the offset to 4x4 hardware tiles.
	// If a user loads a bizarrely sized PNG, this bitwise operation forces
	// the start pointer to the nearest tile boundary, preventing swizzle tearing
	offsetX &= ~3;
	offsetY &= ~3;

	// Calculate the hardware offset in bytes.
	// A 4x4 RGB5A3 tile is 32 bytes.
	int tileRowBytes = (width / 4) * 32;
	int offsetBytes = (offsetY / 4) * tileRowBytes + (offsetX / 4) * 32;

	return (u8*)textureBase + offsetBytes;
}

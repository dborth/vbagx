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
#include "drivers/Platform.h"
#include "drivers/FileSystemDriver.h"

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
static bool rowIsUniform(const uint16_t* buffer, int y, int xStart, int xEnd, uint16_t reference) {
	for (int x = xStart; x < xEnd; x++) {
		if (buffer[SGB_FRAME_WIDTH * y + x] != reference)
			return false;
	}
	return true;
}

bool SgbBorderExtractor::isBorderAreaEmpty(const uint16_t *buffer) {
	uint16_t reference = buffer[0];

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

bool SgbBorderExtractor::processFrame(const uint16_t *buffer, int gbWidth, int gbHeight) {
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
	const char* method = platform->getFileSystem()->getMountPath(GCSettings.LoadMethod);
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
	char* path = (char*)memspace_malloc(length);
	if (path) sprintf(path, "%s%s/%s.png", method, folder, title_buffer);
	return path;
}

uint8_t* BorderManager::load(const char *title, const char *fallback, int &outWidth, int &outHeight) {
	void *png_tmp_buf = memspace_malloc(1024 * 1024);
	char *borderPath = getPNGBorderPath(title);
	int imgWidth = 0, imgHeight = 0;
	uint8_t *rgba = nullptr;
	uint8_t *newBorder = nullptr;

	bool borderLoaded = LoadFile((char*) png_tmp_buf, borderPath, 0, 1024 * 1024, SILENT);
	if (!borderLoaded && fallback) {
		if (borderPath)
			memspace_free(borderPath);
		borderPath = getPNGBorderPath(fallback);
		borderLoaded = LoadFile((char*) png_tmp_buf, borderPath, 0, 1024 * 1024, SILENT);
	}
	if (!borderLoaded)
		goto cleanup;

	if (!PNGGetImageSize((const uint8_t*) png_tmp_buf, &imgWidth, &imgHeight))
		goto cleanup;
	if (imgWidth > 640 || imgHeight > 480)
		goto cleanup;

	rgba = DecodePNGToRGBA8((const uint8_t*) png_tmp_buf, imgWidth, imgHeight);
	if (!rgba)
		goto cleanup;

	// we need the border in non-shared memory because it will cross the menu <> emulator boundary
	newBorder = (uint8_t*)malloc(imgWidth * imgHeight * 4);
	if (!newBorder)
		goto cleanup;

	outWidth = imgWidth;
	outHeight = imgHeight;
	memcpy(newBorder, rgba, imgWidth * imgHeight * 4);

cleanup:
	if (rgba)
		memspace_free(rgba);
	if (png_tmp_buf)
		memspace_free(png_tmp_buf);
	if (borderPath)
		memspace_free(borderPath);

	return newBorder;
}

void BorderManager::save(const void* buffer) {
	char* borderPath = nullptr;
	FILE* f = nullptr;
	uint8_t* rgb24 = nullptr;
	uint8_t* png = nullptr;
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

	rgb24 = (uint8_t*) memspace_malloc(SGB_FRAME_WIDTH * SGB_FRAME_HEIGHT * 3);
	if (!rgb24) goto cleanup;

	// buffer is the raw, linear (not GX-tiled) SGB framebuffer capture,
	// RGB555 pixels with a 258-pixel row stride - just convert to RGB24
	{
		const uint16_t* src = (const uint16_t*) buffer;
		for (int y = 0; y < SGB_FRAME_HEIGHT; y++) {
			const uint16_t* srcRow = src + y * 258;
			uint8_t* dstRow = rgb24 + y * SGB_FRAME_WIDTH * 3;
			for (int x = 0; x < SGB_FRAME_WIDTH; x++) {
				uint16_t color = srcRow[x];
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
	if (borderPath) memspace_free(borderPath);
	if (f) fclose(f);
	if (rgb24) memspace_free(rgb24);
	if (png) memspace_free(png);
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

void GameBorder::setBorder(uint8_t *newPixels, int newWidth, int newHeight) {
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

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
#include "utils/pngu.h"
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

u16* BorderManager::load(const char *title, const char *fallback, int &outWidth, int &outHeight) {
	void *png_tmp_buf = mem1_malloc(1024 * 1024);
	char *borderPath = getPNGBorderPath(title);
	PNGUPROP imgProp;
	IMGCTX ctx = NULL;
	u16 *newBorder = NULL;
	int allocWidth, allocHeight;

	bool borderLoaded = LoadFile((char*) png_tmp_buf, borderPath, 0, 1024 * 1024, SILENT);
	if (!borderLoaded && fallback) {
		if (borderPath)
			mem1_free(borderPath);
		borderPath = getPNGBorderPath(fallback);
		borderLoaded = LoadFile((char*) png_tmp_buf, borderPath, 0, 1024 * 1024, SILENT);
	}
	if (!borderLoaded)
		goto cleanup;

	ctx = PNGU_SelectImageFromBuffer(png_tmp_buf);
	if (!ctx)
		goto cleanup;

	if (PNGU_GetImageProperties(ctx, &imgProp) != PNGU_OK)
		goto cleanup;
	if (imgProp.imgWidth > 640 || imgProp.imgHeight > 480)
		goto cleanup;

	
	// PNGU_DecodeTo4x4RGB555 writes in 4x4 tiles; pad up to a tile boundary so an 
	// odd-sized user PNG can't decode past the end of the buffer
	allocWidth = (imgProp.imgWidth + 3) & ~3;
	allocHeight = (imgProp.imgHeight + 3) & ~3;
	newBorder = (u16*) malloc(allocWidth * allocHeight * 2);
	
	if (!newBorder)
		goto cleanup;

	if (PNGU_DecodeTo4x4RGB555(ctx, imgProp.imgWidth, imgProp.imgHeight, newBorder) != PNGU_OK) {
		free(newBorder);
		newBorder = NULL;
		goto cleanup;
	}

	outWidth = imgProp.imgWidth;
	outHeight = imgProp.imgHeight;

cleanup:
	if (png_tmp_buf)
		mem1_free(png_tmp_buf);
	if (borderPath)
		mem1_free(borderPath);
	if (ctx)
		PNGU_ReleaseImageContext(ctx);

	return newBorder;
}

void BorderManager::save(const void* buffer) {
	char* borderPath = NULL;
	FILE* f = NULL;
	void* rgba8 = NULL;
	IMGCTX pngContext = NULL;

	int err;

	struct stat s;
	borderPath = getPNGBorderPath(NULL);

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

	rgba8 = mem1_malloc(SGB_FRAME_WIDTH * SGB_FRAME_HEIGHT * 3);
	if (!rgba8) goto cleanup;
	pngContext = PNGU_SelectImageFromBuffer(rgba8);
	if (pngContext == NULL) goto cleanup;

	if(PNGU_EncodeFromLinearRGB555(pngContext, SGB_FRAME_WIDTH, SGB_FRAME_HEIGHT, buffer, 258) != PNGU_OK) goto cleanup;
	fwrite(rgba8, 1, SGB_FRAME_WIDTH * SGB_FRAME_HEIGHT * 3, f);

	cleanup:
	if (borderPath) mem1_free(borderPath);
	if (f) fclose(f);
	if (rgba8) mem1_free(rgba8);
	if (pngContext) PNGU_ReleaseImageContext(pngContext);
}

GameBorder::GameBorder() :
	pixels(NULL), width(0), height(0), needsTextureSync(false) {
}

GameBorder::~GameBorder() {
	clear();
}

void GameBorder::clear() {
	if (pixels) {
		free(pixels);
		pixels = NULL;
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
	return pixels != NULL;
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

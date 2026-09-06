/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * gameborder.h
 *
 ****************************************************************************/

#ifndef _GAMEBORDER_H_
#define _GAMEBORDER_H_

#include <malloc.h>
#include <string.h>

// Layout of the 256x224 frame the GB core renders when the user has
// SGBBORDER_FROMGAME selected for an SGB-flagged game: a 160x144 GB screen
// centered inside a border area the *game* paints, not us. vbasupport.cpp's
// gbBorderLineSkip/ColumnSkip/RowSkip configure the core to match this
// exact layout -- keep them in sync if any of these ever change.
static const int SGB_FRAME_WIDTH   = 256;
static const int SGB_FRAME_HEIGHT  = 224;
static const int SGB_SCREEN_WIDTH  = 160;
static const int SGB_SCREEN_HEIGHT = 144;
static const int SGB_BORDER_TOP    = 40;
static const int SGB_BORDER_LEFT   = 48;
static const int SGB_BORDER_BOTTOM = SGB_FRAME_HEIGHT - SGB_BORDER_TOP - SGB_SCREEN_HEIGHT;
static const int SGB_BORDER_RIGHT  = SGB_FRAME_WIDTH - SGB_BORDER_LEFT - SGB_SCREEN_WIDTH;

class SgbBorderExtractor {
private:
	bool isActive;
	int scanThrottle;

	// Checks if the SGB border perimeter is empty (unrendered)
	bool isBorderAreaEmpty(const uint16_t* buffer);

public:
	SgbBorderExtractor();

	// Resets monitoring state during ROM initialization
	void reset(bool isSgbGame, bool borderAlreadyLoaded);

	// The hot-loop hook. Returns true if a border was successfully scraped this frame.
	bool processFrame(const uint16_t* buffer, int gbWidth, int gbHeight);
};

class BorderManager {
public:
	// Returns flat, row-major RGBA8 pixels (decoded straight from the PNG,
	// no platform texture layout applied). Ownership passes to the caller.
	static uint8_t* load(const char* title, const char* fallback, int& outWidth, int& outHeight);
	static void save(const void* buffer);
private:
	static char* getPNGBorderPath(const char* title);
};

// Platform-agnostic border state: just the decoded RGBA8 pixels.
class GameBorder {
private:
	uint8_t* pixels; // RGBA8, width * height * 4 bytes
	int width;
	int height;
	bool needsTextureSync;

public:
	GameBorder();
	~GameBorder();

	// Wipes active memory during menu teardowns
	void clear();

	// Takes ownership of newly loaded/decoded RGBA8 pixels
	void setBorder(uint8_t* newPixels, int newWidth, int newHeight);

	bool hasBorder() const;

	int getWidth() const { return width; }
	int getHeight() const { return height; }

	// Raw RGBA8 source pixels. Callers own converting/uploading these into
	// their own platform texture representation.
	const uint8_t* getPixelsRGBA8() const { return pixels; }

	// One-shot flag: true until the driver holding the live texture has
	// synced these pixels into it. The driver clears it via markSynced().
	bool needsSync() const { return needsTextureSync; }
	void markSynced() { needsTextureSync = false; }
};

extern SgbBorderExtractor sgbBorderExtractor;
extern GameBorder gameBorder;

#endif

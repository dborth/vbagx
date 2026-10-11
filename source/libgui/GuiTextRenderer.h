/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiTextRenderer.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <map>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "Gui.h"

// Legacy Text Styling Constants
#define GUI_TEXT_NULL               0x0000
#define GUI_TEXT_JUSTIFY_LEFT       0x0001
#define GUI_TEXT_JUSTIFY_CENTER     0x0002
#define GUI_TEXT_JUSTIFY_RIGHT      0x0004
#define GUI_TEXT_JUSTIFY_MASK       0x000f

#define GUI_TEXT_ALIGN_TOP          0x0010
#define GUI_TEXT_ALIGN_MIDDLE       0x0020
#define GUI_TEXT_ALIGN_BOTTOM       0x0040
#define GUI_TEXT_ALIGN_BASELINE     0x0080
#define GUI_TEXT_ALIGN_GLYPH_TOP    0x0100
#define GUI_TEXT_ALIGN_GLYPH_MIDDLE 0x0200
#define GUI_TEXT_ALIGN_GLYPH_BOTTOM 0x0400
#define GUI_TEXT_ALIGN_MASK         0x0ff0

#define GUI_TEXT_STYLE_UNDERLINE    0x1000
#define GUI_TEXT_STYLE_STRIKE       0x2000
#define GUI_TEXT_STYLE_MASK         0xf000

const PixelColor black = {0, 0, 0, 255};

//!Per-pixel-size font metrics used for text-block alignment/positioning.
//!\ingroup grp_core
struct FontOffset {
	int16_t ascender; //!< Font ascender for the current size (distance above the baseline), in design pixels
	int16_t descender; //!< Font descender for the current size (negative: distance below the baseline), in design pixels
	int16_t max; //!< Height above the baseline of the tallest glyph in the measured string (see GuiTextRenderer::getOffset())
	int16_t min; //!< Depth below the baseline of the lowest glyph in the measured string (see GuiTextRenderer::getOffset())
};

//!Cached per-glyph metrics and rasterized texture, keyed by (pixel size, char code) in GuiTextRenderer::fontData.
//!\ingroup grp_core
struct GlyphData {
	int16_t renderOffsetX; //!< Horizontal offset from the pen position to the bitmap's left edge
	uint16_t glyphAdvanceX; //!< Horizontal pen advance after this glyph
	uint16_t glyphAdvanceY; //!< Vertical pen advance after this glyph
	uint32_t glyphIndex; //!< FreeType glyph index

	//!Pixel dims of the rasterized bitmap/texture, at uiScale - always
	//!>= drawWidth/drawHeight, and equal to them when uiScale is 1.0.
	uint16_t textureWidth;
	uint16_t textureHeight; //!< Height counterpart of textureWidth

	//!Design-pixel dims to actually draw the quad at (textureWidth/Height
	//!divided back down by uiScale) - keeps text laid out and sized
	//!identically to uiScale 1.0 while the texture itself is crisper.
	uint16_t drawWidth;
	uint16_t drawHeight; //!< Height counterpart of drawWidth

	int16_t renderOffsetY; //!< Height of the bitmap's top edge above the baseline
	int16_t renderOffsetMax; //!< Height of the glyph bitmap above the baseline (feeds FontOffset::max)
	int16_t renderOffsetMin; //!< Depth of the glyph bitmap below the baseline (feeds FontOffset::min)

	void* texture; //!< Abstracted texture pointer
};

//!FreeType2-based glyph shaping/caching. Shapes and caches glyphs per
//!pixel size, then delegates only the final rasterized-quad draw to a
//!GlyphRenderer - GuiText calls through this rather than touching
//!FreeType or a platform texture directly.
//!\ingroup grp_core
class GuiTextRenderer {
private:
	FT_Library ftLibrary;
	FT_Face ftFace;
	int16_t currentPixelSize;
	bool ftKerningEnabled;

	GlyphRenderer* renderer;

	struct ftData {
		FontOffset align;
		std::map<wchar_t, GlyphData> charMap;
	};

	std::map<int16_t, ftData> fontData;

	// Internal Calculations
	int16_t getStyleOffsetWidth(uint16_t width, uint32_t format);
	int16_t getStyleOffsetHeight(FontOffset* offset, uint32_t format);
	void drawTextFeature(int16_t x, int16_t y, uint16_t width, FontOffset* offsetData, uint32_t format, const PixelColor& color);

	// Font Management
	void unloadFont();
	GlyphData* cacheGlyphData(wchar_t charCode, int16_t pixelSize);

	// Physical-pixels-per-design-pixel glyphs are rasterized at (see
	// VideoDriver::getUIScale()).
	float uiScale;

public:
	//!\param fontBuffer TTF/OTF font data - must remain valid for the
	//!lifetime of this GuiTextRenderer, FreeType keeps a pointer into it
	//!\param bufferSize Length of fontBuffer in bytes
	//!\param glyphRenderer Platform renderer rasterized glyph quads are drawn through
	//!\param uiScale Physical-pixels-per-design-pixel to rasterize glyphs at
	//!(pass VideoDriver::getUIScale()). Sizes, metrics and layout stay in
	//!design pixels; only the glyph textures get sharper. 1.0 rasterizes at
	//!the design size.
	GuiTextRenderer(const uint8_t* fontBuffer, FT_Long bufferSize, GlyphRenderer* glyphRenderer, float uiScale = 1.0f);
	~GuiTextRenderer();

	//!Selects the pixel size subsequent drawText()/getWidth()/getHeight()
	//!calls use. Shaped glyphs are cached per size, so switching sizes
	//!repeatedly doesn't re-shape glyphs already seen at that size.
	void setPixelSize(int16_t pixelSize);

	// Core Drawing Signatures
	//!Draws text at (x, y) using the current pixel size.
	//!\param x Left edge, in pixels
	//!\param y Top edge, in pixels
	//!\param text Text to draw
	//!\param color Text color
	//!\param renderFlags Bitmask of GUI_TEXT_JUSTIFY_*/GUI_TEXT_ALIGN_*/GUI_TEXT_STYLE_* flags
	//!\return the drawn text's width in pixels
	uint16_t drawText(int16_t x, int16_t y, const wchar_t* text, PixelColor color = black, uint32_t renderFlags = 0);
	//!\overload
	uint16_t drawText(int16_t x, int16_t y, const char* text, PixelColor color = black, uint32_t renderFlags = 0);

	// Dimensions & Offsets
	//!\return text width in pixels at the current pixel size
	uint16_t getWidth(const wchar_t* text);
	//!\overload
	uint16_t getWidth(const char* text);
	//!\return text height in pixels at the current pixel size
	uint16_t getHeight(const wchar_t* text);
	//!\overload
	uint16_t getHeight(const char* text);

	//!\param text Text to measure
	//!\param offset Filled with the ascender/descender/max/min metrics for text at the current pixel size
	void getOffset(const wchar_t* text, FontOffset* offset);

	// Utilities
	//!\return a newly-allocated wide-char copy of a UTF-8 string p - caller owns the result (delete[])
	static wchar_t* charToWideChar(const char* p);
};

extern GuiTextRenderer *fontSystem;

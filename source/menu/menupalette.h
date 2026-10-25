/****************************************************************************
 * Visual Boy Advance GX
 *
 * menupalette.h
 *
 * The colors that give the app its identity, and nothing else: a primary hue
 * for the chrome, an accent for focus and selection, and a gray secondary for
 * surfaces. Nothing here touches a platform API, so it can be built on a host.
 * theme.cpp turns the palette into the GuiTheme the shared libgui widgets read;
 * menustyle.h builds the menus' own shape styles from it. Sister emulators
 * change MenuPaletteDefault() and keep the rest.
 ***************************************************************************/
#pragma once
#include "libgui/GuiPaint.h"

static inline PixelColor MenuRGBA(int r, int g, int b, int a = 255)
{
	PixelColor c = { (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a };
	return c;
}

//!Everything that differs between the emulators' themes
struct MenuPalette {
	// chrome: the header and footer bars (light edge at the top of the gradient)
	PixelColor chromeLight, chromeDark, chromeEdge;
	// page background, a vertical gradient behind everything
	PixelColor backgroundTop, backgroundBottom;
	// highlight: focused outline, selected-row outline, scrollbar thumb when grabbed
	PixelColor accent, accentDark;
	PixelColor accentTint;		// selected-row fill
	PixelColor glow;			// halo around a focused button; must read on the chrome
	// the gray secondary: cards, outlines, resting controls
	PixelColor surface, surfaceRaised, outline, outlineStrong;
	PixelColor text, textMuted, textOnAccent;
	// what is being edited or pointed at (the highlighted row, the control on the controller picture)
	PixelColor mark;
};

//!Visual Boy Advance GX: purple chrome, gray secondary
static inline MenuPalette MenuPaletteDefault()
{
	MenuPalette p;
	p.chromeLight    = MenuRGBA(104, 22, 122);
	p.chromeDark     = MenuRGBA(66, 4, 80);
	p.chromeEdge     = MenuRGBA(128, 58, 142);
	p.backgroundTop    = MenuRGBA(230, 230, 230);
	p.backgroundBottom = MenuRGBA(200, 200, 200);
	p.accent         = MenuRGBA(130, 36, 152);
	p.accentDark     = MenuRGBA(92, 12, 110);
	p.accentTint     = MenuRGBA(246, 232, 250);
	p.glow           = MenuRGBA(240, 208, 255, 170);
	p.surface        = MenuRGBA(250, 250, 250);
	p.surfaceRaised  = MenuRGBA(255, 255, 255);
	p.outline        = MenuRGBA(220, 220, 220);
	p.outlineStrong  = MenuRGBA(160, 160, 160);
	p.text           = MenuRGBA(30, 30, 30);
	p.textMuted      = MenuRGBA(118, 118, 118);
	p.textOnAccent   = MenuRGBA(255, 255, 255);
	p.mark           = p.accent;
	return p;
}

//!The palette in use
static inline MenuPalette& MenuPaletteCurrent()
{
	static MenuPalette palette = MenuPaletteDefault();
	return palette;
}

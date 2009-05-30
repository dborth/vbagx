#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../System.h"
#include "../NLS.h"
#include "gb.h"
#include "gbCheats.h"
#include "gbGlobals.h"
#include "vba.h"
#include "../ngc/menu.h"

#define CARLLOG

bool ColorizeGameboy = true;

// The 14-colour palette for a monochrome gameboy game
// bg black, bg dark, bg light, bg white, window black, window dark, window light, window white,
// obj0 dark, obj0 light, obj0 white, obj1 dark, obj1 light, obj1 white
// Only the user sets these colours from emulator menu, and it is only read
// by the palette setting functions.
u16 systemMonoPalette[14];

// quickly map the 12 possible indices to 15-bit colours
// bg index 0..3, obj0 index 0..3, obj1 index 0..3
// This is set internally by the palette setting functions, and used by the
// drawing functions.
//u16 gbPalette[12];

static bool HadBgPal = false, HadObj0Pal = false, HadObj1Pal = false;


const float BL[4] = {0.1f, 0.4f, 0.7f, 1.0f};

// Averages colour with white
// factor is from 0 to 1. 0 = no extra whiteness, 1 = 100% whiteness
u16 changeColourWhiteness(u16 rgb, float factor) {
	if (factor==0.0f) return rgb;
	int r = (rgb & 0x1f);
	r = (r * (1-factor))+(0x1f*factor);
	if (r>31) { 
	  r=31; 
	}
	int g = ((rgb >> 5) & 0x1f);
	g = (g * (1-factor))+(0x1f*factor);
	if (g>31) { 
	  g=31; 
	}
	int b = ((rgb >> 10) & 0x1f);
	b = (b * (1-factor))+(0x1f*factor);
	if (b>31) { 
	  b=31; 
	}
	return r | (g << 5) | (b << 10);
}

// Brightens or darkens a colour
// factor is a multiple of normal brightness
u16 changeColourBrightness(u16 rgb, float factor) {
	if (factor==1.0f) return rgb;
	int bonus = 0;
	int r = (rgb & 0x1f);
	r = r * factor;
	if (r>31) { 
	  r=31; 
	  bonus+=(r-30)/2;
	}
	int g = ((rgb >> 5) & 0x1f);
	g = g * factor;
	if (g>31) { 
	  g=31; 
	  bonus+=(g-30)/2;
	}
	int b = ((rgb >> 10) & 0x1f);
	b = b * factor;
	if (b>31) { 
	  b=31; 
	  bonus+=(b-30)/2;
	}
	r+=bonus;
	g+=bonus;
	b+=bonus;
	if (r>31) r=31;
	if (g>31) g=31;
	if (b>31) b=31;
	return r | (g << 5) | (b << 10);
}

u8 oldBgp = 0xFC;

// Sets the brightnesses of both the background and window palettes
void gbSetBGPalette(u8 value, bool ColoursChanged=false) {
  static u8 DarkestToBrightestIndex[4] = {3, 2, 1, 0};
  static u8 Darkness[4] = {0, 1, 2, 3};
  static float BrightnessForBrightest=1, BrightnessForDarkest=0.1; // brightness of palette when indexes are right
  
  // darkness of each index (0 = white, 3 = black)
  gbBgp[0] = value & 0x03;
  gbBgp[1] = (value & 0x0c)>>2;
  gbBgp[2] = (value & 0x30)>>4;
  gbBgp[3] = (value & 0xc0)>>6;
  // Don't mess with the palette unless we have a Mono gameboy and either the colours changed
  // or the brightness palette changed.
  if ((value==oldBgp && !ColoursChanged) || gbCgbMode || gbSgbMode || !ColorizeGameboy) return;
  bool dup = false;
  int dupDarkness = -1;
  if (value!=oldBgp) {
#ifdef CARLLOG
	  const char DN[5] = "3210";
      log("Bg Pal: %c %c %c %c", DN[gbObp0[0]], DN[gbObp0[1]], DN[gbObp0[2]], DN[gbObp0[3]]);
#endif
	  // check for duplicates
	  for (int i=0; i<=2; i++)
		for (int j=i+1; j<=3; j++)
		  if (gbBgp[i]==gbBgp[j]) {
			dup=true;
			dupDarkness = gbBgp[i];
			break;
		  }
	  // We haven't had a full palette yet, so guess...
	  if (dup && !HadBgPal) {
		int index;
	    if (gbBgp[0]>gbBgp[3]) {
			for (int Colour=0; Colour<=3; Colour++) {
				index = Colour;
				DarkestToBrightestIndex[Colour]=index;
				Darkness[Colour]=gbBgp[index];
			}
		} else {
			for (int Colour=0; Colour<=3; Colour++) {
				index = 3-Colour;
				DarkestToBrightestIndex[Colour]=index;
				Darkness[Colour]=gbBgp[index];
			}
		}
		// brightness of brightest colour
		BrightnessForBrightest = BL[3-gbBgp[DarkestToBrightestIndex[3]]];
		BrightnessForDarkest = BL[3-gbBgp[DarkestToBrightestIndex[0]]];
	  } else if (!dup) { // no duplicates, therefore a complete palette change
		HadBgPal = true;
		// now we need to map them from darkest to brightest
		int Colour = 0;
		for (int darkness = 3; darkness>=0; darkness--) {
		  for (int index=0; index<=3; index++) {
			if (gbBgp[index]==darkness) {
				DarkestToBrightestIndex[Colour]=index;
				Darkness[Colour]=gbBgp[index];
				Colour++;
				break;
			}
		  }
		}
		// brightness of brightest colour
		BrightnessForBrightest = BL[3-gbBgp[DarkestToBrightestIndex[3]]];
		BrightnessForDarkest = BL[3-gbBgp[DarkestToBrightestIndex[0]]];
	  } else { // duplicates implies fading in or out
		// since we are really trying to fade, not change palette,
		// rely on previous DarkestToBrightest list 
	  }  
  }
  float NewBrightnessForBrightest = BL[3-gbBgp[DarkestToBrightestIndex[3]]];
  float BrightnessFactor;
  float WhitenessFactor = 0;
  // check if they are trying to make the palette whiter
  if (dupDarkness==0) {
	// Multiple colours set to white
	BrightnessFactor = 0;
	float MaxWhiteness = 0;
	int i;
	for (i=0; i<=3; i++) {
		WhitenessFactor+=BL[3-gbBgp[i]]-BL[3-i];
		MaxWhiteness+=BL[3]-BL[3-i];
	}
	WhitenessFactor = WhitenessFactor / (MaxWhiteness+0.4f);
  // check if they are trying to make the palette brighter
  } else if (NewBrightnessForBrightest==1.0f && BrightnessForBrightest==1.0f) {
	float NewBrightnessForDarkest = BL[3-gbBgp[DarkestToBrightestIndex[0]]];
    BrightnessFactor = NewBrightnessForDarkest/BrightnessForDarkest;

  // Perhaps they are trying to make the palette darker
  } else {
    BrightnessFactor = NewBrightnessForBrightest/BrightnessForBrightest;
  }
  if (WhitenessFactor) {
	  // BG colours
	  for (int colour = 0; colour <= 3; colour++) {
		u16 colourRGB = systemMonoPalette[colour];
		float colourBrightness = BL[colour];
		float indexBrightness = BL[3-Darkness[colour]];
		colourRGB = changeColourBrightness(colourRGB, indexBrightness/colourBrightness);
		gbPalette[0+DarkestToBrightestIndex[colour]] = changeColourWhiteness(colourRGB, WhitenessFactor);
	  }
	  // Window colours
	  for (int colour = 0; colour <= 3; colour++) {
		u16 colourRGB = systemMonoPalette[4+colour];
		float colourBrightness = BL[colour];
		float indexBrightness = BL[3-Darkness[colour]];
		colourRGB = changeColourBrightness(colourRGB, indexBrightness/colourBrightness);
		gbPalette[4+DarkestToBrightestIndex[colour]] = changeColourWhiteness(colourRGB, WhitenessFactor);
	  }
  } else {
	  // BG colours
	  for (int colour = 0; colour <= 3; colour++) {
		u16 colourRGB = systemMonoPalette[colour];
		float colourBrightness = BL[colour];
		float indexBrightness = BL[3-Darkness[colour]]*BrightnessFactor;
		gbPalette[0+DarkestToBrightestIndex[colour]] = changeColourBrightness(colourRGB, indexBrightness/colourBrightness);
	  }
	  // Window colours
	  for (int colour = 0; colour <= 3; colour++) {
		u16 colourRGB = systemMonoPalette[4+colour];
		float colourBrightness = BL[colour];
		float indexBrightness = BL[3-Darkness[colour]]*BrightnessFactor;
		gbPalette[4+DarkestToBrightestIndex[colour]] = changeColourBrightness(colourRGB, indexBrightness/colourBrightness);
	  }
  }
}

u8 oldObp0 = 0xFF;

void gbSetObj0Palette(u8 value, bool ColoursChanged = false) {
  static u8 DarkestToBrightestIndex[3] = {3, 2, 1};
  static u8 Darkness[3] = {1, 2, 3};
  static float BrightnessForBrightest, BrightnessForDarkest; // brightness of palette when indexes are right
  
  // darkness of each index (0 = white, 3 = black)
  gbObp0[0] = value & 0x03;
  gbObp0[1] = (value & 0x0c)>>2;
  gbObp0[2] = (value & 0x30)>>4;
  gbObp0[3] = (value & 0xc0)>>6;
  
  // Don't mess with the palette unless we have a Mono gameboy and either the colours changed
  // or the brightness palette changed.
  if ((value==oldObp0 && !ColoursChanged) || gbCgbMode || gbSgbMode || !ColorizeGameboy) return;
  bool dup = false;
  int dupDarkness = -1;
  if (value!=oldObp0) {
#ifdef CARLLOG
	  const char DN[5] = "3210";
      log("Obj0 Pal:         %c %c %c", DN[gbObp0[1]], DN[gbObp0[2]], DN[gbObp0[3]]);
#endif
	  // check for duplicates
	  for (int i=1; i<=2; i++)
		for (int j=i+1; j<=3; j++)
		  if (gbObp0[i]==gbObp0[j]) {
			dup=true;
			dupDarkness = gbObp0[i];
			break;
		  }
	  	  // We haven't had a full palette yet, so guess...
	  if (dup && !HadObj1Pal) {
		int index;
	    if (gbObp0[1]>gbObp0[3]) {
			for (int Colour=0; Colour<=2; Colour++) {
				index = Colour+1;
				DarkestToBrightestIndex[Colour]=index;
				Darkness[Colour]=gbObp0[index];
			}
		} else {
			for (int Colour=0; Colour<=2; Colour++) {
				index = 2-Colour+1;
				DarkestToBrightestIndex[Colour]=index;
				Darkness[Colour]=gbObp0[index];
			}
		}
		// brightness of brightest colour
		BrightnessForBrightest = BL[3-gbObp0[DarkestToBrightestIndex[2]]];
		BrightnessForDarkest = BL[3-gbObp0[DarkestToBrightestIndex[0]]];
	  } else if (!dup) { // no duplicates, therefore a complete brightness palette change
		HadObj0Pal = true;
		// now we need to map them from darkest to brightest
		int Colour = 0;
		for (int darkness = 3; darkness>=0; darkness--) {
		  for (int index=1; index<=3; index++) {
			if (gbObp0[index]==darkness) {
				DarkestToBrightestIndex[Colour]=index;
				Darkness[Colour]=gbObp0[index];
				Colour++;
				break;
			}
		  }
		}
		// brightness of brightest colour
		BrightnessForBrightest = BL[3-gbObp0[DarkestToBrightestIndex[2]]];
		BrightnessForDarkest = BL[3-gbObp0[DarkestToBrightestIndex[0]]];
	  } else { // duplicates implies fading in or out
		// since we are really trying to fade, not change palette,
		// rely on previous DarkestToBrightest list 
	  }
  }
  float NewBrightnessForBrightest = BL[3-gbObp0[DarkestToBrightestIndex[2]]];
  float BrightnessFactor;
  float WhitenessFactor = 0;
  // check if they are trying to make the palette whiter
  if (dupDarkness==0) {
	// Multiple colours set to white
	BrightnessFactor = 0;
	float MaxWhiteness = 0;
	int i;
	for (i=1; i<=3; i++) {
		WhitenessFactor+=BL[3-gbObp0[i]]-BL[3-(i-1)];
		MaxWhiteness+=BL[3]-BL[3-(i-1)];
	}
	WhitenessFactor = WhitenessFactor / (MaxWhiteness+0.3f);
  // check if they are trying to make the palette brighter
  } else if (NewBrightnessForBrightest==1.0f && BrightnessForBrightest==1.0f) {
	float NewBrightnessForDarkest = BL[3-gbObp0[DarkestToBrightestIndex[0]]];
    BrightnessFactor = NewBrightnessForDarkest/BrightnessForDarkest;
  } else {
    BrightnessFactor = NewBrightnessForBrightest/BrightnessForBrightest;
  }
  
  if (WhitenessFactor) {
	  for (int colour = 0; colour <= 2; colour++) {
		u16 colourRGB = systemMonoPalette[colour+8];
		float colourBrightness = BL[colour+1];
		float indexBrightness = BL[3-Darkness[colour]];
		colourRGB = changeColourBrightness(colourRGB, indexBrightness/colourBrightness);
		gbPalette[8+DarkestToBrightestIndex[colour]] = changeColourWhiteness(colourRGB, WhitenessFactor);
	  }
  } else {
	  for (int colour = 0; colour <= 2; colour++) {
		u16 colourRGB = systemMonoPalette[colour+8];
		float colourBrightness = BL[colour+1];
		float indexBrightness = BL[3-Darkness[colour]]*BrightnessFactor;
		gbPalette[8+DarkestToBrightestIndex[colour]] = changeColourBrightness(colourRGB, indexBrightness/colourBrightness);
	  }
  }
  gbPalette[8] = 0; // always transparent
}

u8 oldObp1 = 0xFF;

void gbSetObj1Palette(u8 value, bool ColoursChanged = false) {
  static u8 DarkestToBrightestIndex[3] = {3, 2, 1};
  static u8 Darkness[3] = {1, 2, 3};
  static float BrightnessForBrightest, BrightnessForDarkest; // brightness of palette when indexes are right
  
  // darkness of each index (0 = white, 3 = black)
  gbObp1[0] = value & 0x03;
  gbObp1[1] = (value & 0x0c)>>2;
  gbObp1[2] = (value & 0x30)>>4;
  gbObp1[3] = (value & 0xc0)>>6;
  // Don't mess with the palette unless we have a Mono gameboy and either the colours changed
  // or the brightness palette changed.
  if ((value==oldObp1 && !ColoursChanged) || gbCgbMode || gbSgbMode || !ColorizeGameboy) return;
  bool dup = false;
  int dupDarkness = -1;
  if (value!=oldObp1) {
#ifdef CARLLOG
	  const char DN[5] = "3210";
      log("Obj1 Pal:                 %c %c %c", DN[gbObp1[1]], DN[gbObp1[2]], DN[gbObp1[3]]);
#endif
	  // check for duplicates
	  for (int i=1; i<=2; i++)
		for (int j=i+1; j<=3; j++)
		  if (gbObp1[i]==gbObp1[j]) {
			dup=true;
			dupDarkness=gbObp1[i];
			break;
		  }
	  // We haven't had a full palette yet, so guess...
	  if (dup && !HadObj1Pal) {
		int index;
	    if (gbObp1[1]>gbObp1[3]) {
			for (int Colour=0; Colour<=2; Colour++) {
				index = Colour+1;
				DarkestToBrightestIndex[Colour]=index;
				Darkness[Colour]=gbObp1[index];
			}
		} else {
			for (int Colour=0; Colour<=2; Colour++) {
				index = 2-Colour+1;
				DarkestToBrightestIndex[Colour]=index;
				Darkness[Colour]=gbObp1[index];
			}
		}
		// brightness of brightest colour
		BrightnessForBrightest = BL[3-gbObp1[DarkestToBrightestIndex[2]]];
		BrightnessForDarkest = BL[3-gbObp1[DarkestToBrightestIndex[0]]];
	  } else if (!dup) { // no duplicates, therefore a complete palette change
		HadObj1Pal = true;
		// now we need to map them from darkest to brightest
		int Colour = 0;
		for (int darkness = 3; darkness>=0; darkness--) {
		  for (int index=1; index<=3; index++) {
			if (gbObp1[index]==darkness) {
				DarkestToBrightestIndex[Colour]=index;
				Darkness[Colour]=gbObp1[index];
				Colour++;
				break;
			}
		  }
		}
		// brightness of brightest colour
		BrightnessForBrightest = BL[3-gbObp1[DarkestToBrightestIndex[2]]];
		BrightnessForDarkest = BL[3-gbObp1[DarkestToBrightestIndex[0]]];
	  } else { // duplicates implies fading in or out
		// since we are really trying to fade, not change palette,
		// rely on previous DarkestToBrightest list 
	  }  
  }
  float NewBrightnessForBrightest = BL[3-gbObp1[DarkestToBrightestIndex[2]]];
  float BrightnessFactor;
  float WhitenessFactor = 0;
  // check if they are trying to make the palette whiter
  if (dupDarkness==0) {
	// Multiple colours set to white
	BrightnessFactor = 0;
	float MaxWhiteness = 0;
	int i;
	for (i=1; i<=3; i++) {
		WhitenessFactor+=BL[3-gbObp1[i]]-BL[3-(i-1)];
		MaxWhiteness+=BL[3]-BL[3-(i-1)];
	}
	WhitenessFactor = WhitenessFactor / (MaxWhiteness+0.3f);
  // check if they are trying to make the palette brighter
  } else if (NewBrightnessForBrightest==1.0f && BrightnessForBrightest==1.0f) {
	float NewBrightnessForDarkest = BL[3-gbObp1[DarkestToBrightestIndex[0]]];
    BrightnessFactor = NewBrightnessForDarkest/BrightnessForDarkest;
  } else {
    BrightnessFactor = NewBrightnessForBrightest/BrightnessForBrightest;
  }
  
  if (WhitenessFactor) {
	  for (int colour = 0; colour <= 2; colour++) {
		u16 colourRGB = systemMonoPalette[colour+11];
		float colourBrightness = BL[colour+1];
		float indexBrightness = BL[3-Darkness[colour]];
		colourRGB = changeColourBrightness(colourRGB, indexBrightness/colourBrightness);
		gbPalette[12+DarkestToBrightestIndex[colour]] = changeColourWhiteness(colourRGB, WhitenessFactor);
	  }
  } else {
	  for (int colour = 0; colour <= 2; colour++) {
		u16 colourRGB = systemMonoPalette[colour+11];
		float colourBrightness = BL[colour+1];
		float indexBrightness = BL[3-Darkness[colour]]*BrightnessFactor;
		gbPalette[12+DarkestToBrightestIndex[colour]] = changeColourBrightness(colourRGB, indexBrightness/colourBrightness);
	  }
  }
  gbPalette[12] = 0; // always transparent
}

bool StartColorizing() {
  if ((!GCSettings.colorize) || gbSgbMode || gbCgbMode) return false;
  if (ColorizeGameboy) return true;
  ColorizeGameboy = true;
  gbSetBGPalette(oldBgp);
  gbSetObj0Palette(oldObp0);
  gbSetObj1Palette(oldObp1);
  return true;
}

void StopColorizing() {
  for(int i = 0; i < 12; i++)
    gbPalette[i] = systemGbPalette[gbPaletteOption*12+i];
  ColorizeGameboy = false;
}

// convert 0xRRGGBB to our 15 bit format
u16 Make15Bit(u32 rgb) {
	return ((rgb >> 19) & 0x1F) | (((rgb >> 11) & 0x1F) << 5) | (((rgb >> 3) & 0x1F) << 10);
}

u32 OldObpBright[2]={0}, OldObpMedium[2]={0}, OldObpDark[2]={0};

void gbSetSpritePal(u8 WhichPal, u32 bright, u32 medium, u32 dark) {
  if (!StartColorizing()) return;
  // cancel if we already set to these colours
  if (WhichPal>0) {
	int p = WhichPal-1;
	if (OldObpBright[p]==bright && OldObpMedium[p]==medium && OldObpDark[p]==dark) return;
  }
  int index = 0;
  // check if we are setting both sprite palettes at once
  if (WhichPal==0) {
    gbSetSpritePal(1, bright, medium, dark);
    gbSetSpritePal(2, bright, medium, dark);
    return;
  } else if (WhichPal==1) index = 8; // Obj0
  else if (WhichPal==2) index=11; // Obj1
  // save colours as 15 bit
  systemMonoPalette[index]=Make15Bit(dark);
  systemMonoPalette[index+1]=Make15Bit(medium);
  systemMonoPalette[index+2]=Make15Bit(bright);
  // change the brightness palette
  if (WhichPal==1) {
	u8 old = oldObp0;
	oldObp0 +=1;
	gbSetObj0Palette(old);
  } else if (WhichPal==2) {
	u8 old = oldObp1;
	oldObp1 +=1;
	gbSetObj1Palette(old);
  }
}

void gbSetSpritePal(u8 WhichPal, u32 bright) {
	u8 r = (bright >> 16) & 0xFF;
	u8 g = (bright >> 8) & 0xFF;
	u8 b = (bright >> 0) & 0xFF;
	u32 medium = (((u32)(r*0.7f)) << 16) | (((u32)(g*0.7f)) << 8) | (((u32)(b*0.7f)) << 0);
	u32 dark = (((u32)(r*0.4f)) << 16) | (((u32)(g*0.4f)) << 8) | (((u32)(b*0.4f)) << 0);
	gbSetSpritePal(WhichPal, bright, medium, dark);
}

u32 OldBgBright=0, OldBgMedium=0, OldBgDark=0, OldBgBlack=0;

void gbSetBgPal(u8 WhichPal, u32 bright, u32 medium, u32 dark, u32 black=0x000000) {
  if (!StartColorizing()) return;
  if (OldBgBright==bright && OldBgMedium==medium && OldBgDark==dark && OldBgBlack==black) 
	return;
  int index = 0;
  if (WhichPal==0) {
    gbSetBgPal(1, bright, medium, dark, black);
    gbSetBgPal(2, bright, medium, dark, black);
    return;
  } else if (WhichPal==1) index = 0; // background
  else if (WhichPal==2) index=4; // window
  systemMonoPalette[index]=Make15Bit(black);
  systemMonoPalette[index+1]=Make15Bit(dark);
  systemMonoPalette[index+2]=Make15Bit(medium);
  systemMonoPalette[index+3]=Make15Bit(bright);
  gbSetBGPalette(oldBgp, true);
  OldBgBright = bright;
  OldBgMedium = medium;
  OldBgDark = dark;
  OldBgBlack = black;
}

void gbSetBgPal(u8 WhichPal, u32 bright) {
	u8 r = (bright >> 16) & 0xFF;
	u8 g = (bright >> 8) & 0xFF;
	u8 b = (bright >> 0) & 0xFF;
	u32 medium = (((u32)(r*0.7f)) << 16) | (((u32)(g*0.7f)) << 8) | (((u32)(b*0.7f)) << 0);
	u32 dark = (((u32)(r*0.4f)) << 16) | (((u32)(g*0.4f)) << 8) | (((u32)(b*0.4f)) << 0);
	u32 black = (((u32)(r*0.1f)) << 16) | (((u32)(g*0.1f)) << 8) | (((u32)(b*0.1f)) << 0);
	gbSetBgPal(WhichPal, bright, medium, dark, black);
}

// Set the whole 14-colour palette
void gbSetPalette(u32 RRGGBB[]) {
	gbSetBgPal(1, RRGGBB[0], RRGGBB[1], RRGGBB[2], RRGGBB[3]);
	gbSetBgPal(2, RRGGBB[4], RRGGBB[5], RRGGBB[6], RRGGBB[7]);
	gbSetSpritePal(1, RRGGBB[8], RRGGBB[9], RRGGBB[10]);
	gbSetSpritePal(2, RRGGBB[11], RRGGBB[12], RRGGBB[13]);
}

void gbPaletteReset() {
	HadBgPal = HadObj0Pal = HadObj1Pal = false;
	oldBgp = 0xFC;
	oldObp0 = oldObp1 = 0xFF;
	OldBgBright=0; OldBgMedium=0; OldBgDark=0; OldBgBlack=0;
	for (int i=0; i<=1; i++) {
		OldObpBright[i]=OldObpMedium[i]=OldObpDark[i]=0;
	}
}

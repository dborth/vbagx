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

const float BL[4] = {0.1f, 0.4f, 0.7f, 1.0f};

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
  if ((value==oldBgp && !ColoursChanged) || gbCgbMode || gbSgbMode || !ColorizeGameboy) return;
  // check for duplicates
  bool dup = false;
  for (int i=0; i<=2; i++)
	for (int j=i+1; j<=3; j++)
	  if (gbBgp[i]==gbBgp[j]) {
		dup=true;
		break;
	  }
  if (!dup) { // no duplicates, therefore a complete palette change
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
  float NewBrightnessForBrightest = BL[3-gbBgp[DarkestToBrightestIndex[3]]];
  float BrightnessFactor;
  // check if they are trying to make the palette brighter
  if (NewBrightnessForBrightest==1.0f && BrightnessForBrightest==1.0f) {
	float NewBrightnessForDarkest = BL[3-gbBgp[DarkestToBrightestIndex[0]]];
    BrightnessFactor = NewBrightnessForDarkest/BrightnessForDarkest;
  } else {
    BrightnessFactor = NewBrightnessForBrightest/BrightnessForBrightest;
  }
  
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
  if ((value==oldObp0 && !ColoursChanged) || gbCgbMode || gbSgbMode || !ColorizeGameboy) return;
  // check for duplicates
  bool dup = false;
  for (int i=1; i<=2; i++)
	for (int j=i+1; j<=3; j++)
	  if (gbObp0[i]==gbObp0[j]) {
		dup=true;
		break;
	  }
  if (!dup) { // no duplicates, therefore a complete palette change
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
  float NewBrightnessForBrightest = BL[3-gbObp0[DarkestToBrightestIndex[2]]];
  float BrightnessFactor;
  // check if they are trying to make the palette brighter
  if (NewBrightnessForBrightest==1.0f && BrightnessForBrightest==1.0f) {
	float NewBrightnessForDarkest = BL[3-gbObp0[DarkestToBrightestIndex[0]]];
    BrightnessFactor = NewBrightnessForDarkest/BrightnessForDarkest;
  } else {
    BrightnessFactor = NewBrightnessForBrightest/BrightnessForBrightest;
  }
  
  for (int colour = 0; colour <= 2; colour++) {
	u16 colourRGB = systemMonoPalette[colour+8];
	float colourBrightness = BL[colour+1];
	float indexBrightness = BL[3-Darkness[colour]]*BrightnessFactor;
	gbPalette[8+DarkestToBrightestIndex[colour]] = changeColourBrightness(colourRGB, indexBrightness/colourBrightness);
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
  if ((value==oldObp1 && !ColoursChanged) || gbCgbMode || gbSgbMode || !ColorizeGameboy) return;
  // check for duplicates
  bool dup = false;
  for (int i=1; i<=2; i++)
	for (int j=i+1; j<=3; j++)
	  if (gbObp1[i]==gbObp1[j]) {
		dup=true;
		break;
	  }
  if (!dup) { // no duplicates, therefore a complete palette change
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
  float NewBrightnessForBrightest = BL[3-gbObp1[DarkestToBrightestIndex[2]]];
  float BrightnessFactor;
  // check if they are trying to make the palette brighter
  if (NewBrightnessForBrightest==1.0f && BrightnessForBrightest==1.0f) {
	float NewBrightnessForDarkest = BL[3-gbObp1[DarkestToBrightestIndex[0]]];
    BrightnessFactor = NewBrightnessForDarkest/BrightnessForDarkest;
  } else {
    BrightnessFactor = NewBrightnessForBrightest/BrightnessForBrightest;
  }
  
  for (int colour = 0; colour <= 2; colour++) {
	u16 colourRGB = systemMonoPalette[colour+11];
	float colourBrightness = BL[colour+1];
	float indexBrightness = BL[3-Darkness[colour]]*BrightnessFactor;
	gbPalette[12+DarkestToBrightestIndex[colour]] = changeColourBrightness(colourRGB, indexBrightness/colourBrightness);
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

void gbSetSpritePal(u8 WhichPal, u32 bright, u32 medium, u32 dark) {
  if (!StartColorizing()) return;
  int index = 0;
  if (WhichPal==0) {
    gbSetSpritePal(1, bright, medium, dark);
    gbSetSpritePal(2, bright, medium, dark);
    return;
  } else if (WhichPal==1) index = 8; // Obj0
  else if (WhichPal==2) index=11; // Obj1
  systemMonoPalette[index]=Make15Bit(dark);
  systemMonoPalette[index+1]=Make15Bit(medium);
  systemMonoPalette[index+2]=Make15Bit(bright);
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

void gbSetBgPal(u8 WhichPal, u32 bright, u32 medium, u32 dark, u32 black=0x000000) {
  if (!StartColorizing()) return;
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

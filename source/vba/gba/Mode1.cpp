/*
Mode 1 is a tiled graphics mode, but with background layer 2 supporting scaling and rotation.
There is no layer 3 in this mode.
Layers 0 and 1 can be either 16 colours (with 16 different palettes) or 256 colours. 
There are 1024 tiles available.
Layer 2 is 256 colours and allows only 256 tiles.

*/
#include "GBA.h"
#include "Globals.h"
#include "GBAGfx.h"

static inline bool mode1CheckBlanking() {
  if(DISPCNT & 0x80) {
    int x = 232;
    do {
      lineMix[x] = lineMix[x+1] = lineMix[x+2] = lineMix[x+3] =
      lineMix[x+4] = lineMix[x+5] = lineMix[x+6] = lineMix[x+7] = 0x7fff;
      x -= 8;
    } while(x >= 0);
    gfxLastVCOUNT = VCOUNT;
    return true;
  }
  return false;
}

template <int EFFECT>
static inline void mode1RenderLine_Impl() {
  u16 *palette = (u16 *)paletteRAM;
  if(mode1CheckBlanking()) return;

  if(layerEnable & 0x0100) gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
  if(layerEnable & 0x0200) gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
  if(layerEnable & 0x0400) {
    int changed = gfxBG2Changed;
    if(gfxLastVCOUNT > VCOUNT) changed = 3;
    gfxDrawRotScreen(BG2CNT, BG2X_L, BG2X_H, BG2Y_L, BG2Y_H, BG2PA, BG2PB, BG2PC, BG2PD, gfxBG2X, gfxBG2Y, changed, line2);
  }
  gfxDrawSprites(lineOBJ);

  u32 backdrop = (customBackdropColor == -1) ? (READ16LE(&palette[0]) | 0x30000000) : ((customBackdropColor & 0x7FFF) | 0x30000000);

  // Flattened GPR pointer alignment
  u32* l0 = line0; u32* l1 = line1; u32* l2 = line2;
  u32* lO = lineOBJ; u32* lM = lineMix;

  for(u32 x = 0; x < 240u; ++x) {
    u32 c0 = *l0++; u32 c1 = *l1++; u32 c2 = *l2++; u32 cO = *lO++;
    u32 color = backdrop;
    u8 top = 0x20;

    u8 li1 = (u8)(c1 >> 24);
    u8 li2 = (u8)(c2 >> 24);
    u8 li4 = (u8)(cO >> 24);

    u8 r_12 = li2 ^ ((li1 ^ li2) & -(li1 < li2));
    u8 r = 	li4 ^ ((r_12 ^ li4) & -(r_12 < li4));

    if(c0 < backdrop) {
      color = c0;
      top = 0x01;
    }

    if(r < (u8)(color >> 24)) {
      if(r == li1) { color = c1; top = 0x02; }
      else if(r == li2) { color = c2; top = 0x04; }
      else if(r == li4) { color = cO; top = 0x10; }
    }

    if((top & 0x10) && (color & 0x00010000)) {
      u32 back = backdrop;
      u8 top2 = 0x20;

      u8 li0 = (u8)(c0 >> 24);
      u8 r_01 = li1 ^ ((li0 ^ li1) & -(li0 < li1));
      u8 r_back = li2 ^ ((r_01 ^ li2) & -(r_01 < li2));

      if(r_back < (u8)(back >> 24)) {
        if(r_back == li0) { back = c0; top2 = 0x01; }
        else if(r_back == li1) { back = c1; top2 = 0x02; }
        else if(r_back == li2) { back = c2; top2 = 0x04; }
      }

      if(top2 & (BLDMOD>>8)) {
        color = gfxAlphaBlend(color, back, coeff[COLEV & 0x1F], coeff[(COLEV >> 8) & 0x1F]);
      } else {
        if (EFFECT == 2) {
          if(BLDMOD & top) color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
        } else if (EFFECT == 3) {
          if(BLDMOD & top) color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
        }
      }
    }
    *lM++ = color;
  }
  gfxBG2Changed = 0;
  gfxLastVCOUNT = VCOUNT;
}

template <int EFFECT>
static inline void mode1RenderLineNoWindow_Impl() {
  u16 *palette = (u16 *)paletteRAM;
  if(mode1CheckBlanking()) return;

  if(layerEnable & 0x0100) gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
  if(layerEnable & 0x0200) gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
  if(layerEnable & 0x0400) {
    int changed = gfxBG2Changed;
    if(gfxLastVCOUNT > VCOUNT) changed = 3;
    gfxDrawRotScreen(BG2CNT, BG2X_L, BG2X_H, BG2Y_L, BG2Y_H, BG2PA, BG2PB, BG2PC, BG2PD, gfxBG2X, gfxBG2Y, changed, line2);
  }
  gfxDrawSprites(lineOBJ);

  u32 backdrop = (customBackdropColor == -1) ? (READ16LE(&palette[0]) | 0x30000000) : ((customBackdropColor & 0x7FFF) | 0x30000000);

  u32* l0 = line0; u32* l1 = line1; u32* l2 = line2;
  u32* lO = lineOBJ; u32* lM = lineMix;

  for(u32 x = 0; x < 240u; ++x) {
    u32 c0 = *l0++; u32 c1 = *l1++; u32 c2 = *l2++; u32 cO = *lO++;
    u32 color = backdrop;
    u8 top = 0x20;

    u8 li1 = (u8)(c1>>24);
    u8 li2 = (u8)(c2>>24);
    u8 li4 = (u8)(cO>>24);

    u8 r_12 = li2 ^ ((li1 ^ li2) & -(li1 < li2));
    u8 r = 	li4 ^ ((r_12 ^ li4) & -(r_12 < li4));

    if(c0 < backdrop) {
      color = c0;
      top = 0x01;
    }

    if(r < (u8)(color >> 24)) {
      if(r == li1) { color = c1; top = 0x02; }
      else if(r == li2) { color = c2; top = 0x04; }
      else if(r == li4) { color = cO; top = 0x10; }
    }

    if(!(color & 0x00010000)) {
      if (EFFECT == 1) {
        if(top & BLDMOD) {
          u32 back = backdrop;
          u8 top2 = 0x20;

          if((top != 0x01) && (u8)(c0>>24) < (u8)(back >> 24)) { back = c0; top2 = 0x01; }
          if((top != 0x02) && (u8)(c1>>24) < (u8)(back >> 24)) { back = c1; top2 = 0x02; }
          if((top != 0x04) && (u8)(c2>>24) < (u8)(back >> 24)) { back = c2; top2 = 0x04; }
          if((top != 0x10) && (u8)(cO>>24) < (u8)(back >> 24)) { back = cO; top2 = 0x10; }

          if(top2 & (BLDMOD>>8)) color = gfxAlphaBlend(color, back, coeff[COLEV & 0x1F], coeff[(COLEV >> 8) & 0x1F]);
        }
      } else if (EFFECT == 2) {
        if(BLDMOD & top) color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
      } else if (EFFECT == 3) {
        if(BLDMOD & top) color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
      }
    } else {
      u32 back = backdrop;
      u8 top2 = 0x20;

      u8 li0 = (u8)(c0>>24);
      u8 r_01 = li1 ^ ((li0 ^ li1) & -(li0 < li1));
      u8 r_back = li2 ^ ((r_01 ^ li2) & -(r_01 < li2));

      if(r_back < (u8)(back >> 24)) {
        if(r_back == li0) { back = c0; top2 = 0x01; }
        else if(r_back == li1) { back = c1; top2 = 0x02; }
        else if(r_back == li2) { back = c2; top2 = 0x04; }
      }

      if(top2 & (BLDMOD>>8)) {
        color = gfxAlphaBlend(color, back, coeff[COLEV & 0x1F], coeff[(COLEV >> 8) & 0x1F]);
      } else {
        if (EFFECT == 2) {
          if(BLDMOD & top) color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
        } else if (EFFECT == 3) {
          if(BLDMOD & top) color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
        }
      }
    }
    *lM++ = color;
  }
  gfxBG2Changed = 0;
  gfxLastVCOUNT = VCOUNT;
}

template <int EFFECT>
static inline void mode1RenderLineAll_Impl() {
  u16 *palette = (u16 *)paletteRAM;
  if(mode1CheckBlanking()) return;

  bool inWindow0 = false;
  bool inWindow1 = false;

  if(layerEnable & 0x2000) {
    u8 v0 = WIN0V >> 8; u8 v1 = WIN0V & 255;
    inWindow0 = ((v0 == v1) && (v0 >= 0xe8));
    inWindow0 |= (v1 >= v0) ? (VCOUNT >= v0 && VCOUNT < v1) : (VCOUNT >= v0 || VCOUNT < v1);
  }
  if(layerEnable & 0x4000) {
    u8 v0 = WIN1V >> 8; u8 v1 = WIN1V & 255;
    inWindow1 = ((v0 == v1) && (v0 >= 0xe8));
    inWindow1 |= (v1 >= v0) ? (VCOUNT >= v0 && VCOUNT < v1) : (VCOUNT >= v0 || VCOUNT < v1);
  }

  if(layerEnable & 0x0100) gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
  if(layerEnable & 0x0200) gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
  if(layerEnable & 0x0400) {
    int changed = gfxBG2Changed;
    if(gfxLastVCOUNT > VCOUNT) changed = 3;
    gfxDrawRotScreen(BG2CNT, BG2X_L, BG2X_H, BG2Y_L, BG2Y_H, BG2PA, BG2PB, BG2PC, BG2PD, gfxBG2X, gfxBG2Y, changed, line2);
  }

  gfxDrawSprites(lineOBJ);
  gfxDrawOBJWin(lineOBJWin);

  u32 backdrop = (customBackdropColor == -1) ? (READ16LE(&palette[0]) | 0x30000000) : ((customBackdropColor & 0x7FFF) | 0x30000000);

  u8 inWin0Mask = WININ & 0xFF;
  u8 inWin1Mask = WININ >> 8;
  u8 outMask = WINOUT & 0xFF;

  u32* l0 = line0; u32* l1 = line1; u32* l2 = line2;
  u32* lO = lineOBJ; u32* lOW = lineOBJWin; u32* lM = lineMix;
  const bool* w0 = gfxInWin0; const bool* w1 = gfxInWin1;

  for(u32 x = 0; x < 240u; ++x) {
    u32 c0 = *l0++; u32 c1 = *l1++; u32 c2 = *l2++; u32 cO = *lO++;
    u32 cOW = *lOW++; bool cw0 = *w0++; bool cw1 = *w1++;
    u32 color = backdrop;
    u8 top = 0x20;
    u8 mask = outMask;

    if(!(cOW & 0x80000000)) mask = WINOUT >> 8;
    if(inWindow1 && cw1) mask = inWin1Mask;
    if(inWindow0 && cw0) mask = inWin0Mask;

    if((mask & 1) && c0 < backdrop) { color = c0; top = 0x01; }
    if((mask & 2) && (u8)(c1>>24) < (u8)(color >> 24)) { color = c1; top = 0x02; }
    if((mask & 4) && (u8)(c2>>24) < (u8)(color >> 24)) { color = c2; top = 0x04; }
    if((mask & 16) && (u8)(cO>>24) < (u8)(color >> 24)) { color = cO; top = 0x10; }

    if(color & 0x00010000) {
      u32 back = backdrop;
      u8 top2 = 0x20;

      if((mask & 1) && (u8)(c0>>24) < (u8)(backdrop >> 24)) { back = c0; top2 = 0x01; }
      if((mask & 2) && (u8)(c1>>24) < (u8)(back >> 24)) { back = c1; top2 = 0x02; }
      if((mask & 4) && (u8)(c2>>24) < (u8)(back >> 24)) { back = c2; top2 = 0x04; }

      if(top2 & (BLDMOD>>8)) {
        color = gfxAlphaBlend(color, back, coeff[COLEV & 0x1F], coeff[(COLEV >> 8) & 0x1F]);
      } else {
        if (EFFECT == 2) {
          if(BLDMOD & top) color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
        } else if (EFFECT == 3) {
          if(BLDMOD & top) color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
        }
      }
    } else if(mask & 32) {
      if (EFFECT == 1) {
        if(top & BLDMOD) {
          u32 back = backdrop;
          u8 top2 = 0x20;

          if((mask & 1) && (top != 0x01) && (u8)(c0>>24) < (u8)(backdrop >> 24)) { back = c0; top2 = 0x01; }
          if((mask & 2) && (top != 0x02) && (u8)(c1>>24) < (u8)(back >> 24)) { back = c1; top2 = 0x02; }
          if((mask & 4) && (top != 0x04) && (u8)(c2>>24) < (u8)(back >> 24)) { back = c2; top2 = 0x04; }
          if((mask & 16) && (top != 0x10) && (u8)(cO>>24) < (u8)(back >> 24)) { back = cO; top2 = 0x10; }

          if(top2 & (BLDMOD>>8)) color = gfxAlphaBlend(color, back, coeff[COLEV & 0x1F], coeff[(COLEV >> 8) & 0x1F]);
        }
      } else if (EFFECT == 2) {
        if(BLDMOD & top) color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
      } else if (EFFECT == 3) {
        if(BLDMOD & top) color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
      }
    }
    *lM++ = color;
  }
  gfxBG2Changed = 0;
  gfxLastVCOUNT = VCOUNT;
}

void mode1RenderLine() {
  switch((BLDMOD >> 6) & 3) {
    case 0: mode1RenderLine_Impl<0>(); break;
    case 1: mode1RenderLine_Impl<1>(); break;
    case 2: mode1RenderLine_Impl<2>(); break;
    case 3: mode1RenderLine_Impl<3>(); break;
  }
}

void mode1RenderLineNoWindow() {
  switch((BLDMOD >> 6) & 3) {
    case 0: mode1RenderLineNoWindow_Impl<0>(); break;
    case 1: mode1RenderLineNoWindow_Impl<1>(); break;
    case 2: mode1RenderLineNoWindow_Impl<2>(); break;
    case 3: mode1RenderLineNoWindow_Impl<3>(); break;
  }
}

void mode1RenderLineAll() {
  switch((BLDMOD >> 6) & 3) {
    case 0: mode1RenderLineAll_Impl<0>(); break;
    case 1: mode1RenderLineAll_Impl<1>(); break;
    case 2: mode1RenderLineAll_Impl<2>(); break;
    case 3: mode1RenderLineAll_Impl<3>(); break;
  }
}

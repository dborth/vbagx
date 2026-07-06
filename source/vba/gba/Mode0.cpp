/*
Mode 0 is the tiled graphics mode, with all the layers available.
There is no rotation or scaling in this mode.
It can be either 16 colours (with 16 different palettes) or 256 colors.
There are 1024 tiles available.
*/
#include "GBA.h"
#include "Globals.h"
#include "GBAGfx.h"

static inline bool mode0CheckBlanking() {
  if(DISPCNT & 0x80) {
    int x = 232;
    do {
      lineMix[x] = lineMix[x+1] = lineMix[x+2] = lineMix[x+3] =
      lineMix[x+4] = lineMix[x+5] = lineMix[x+6] = lineMix[x+7] = 0x7fff;
      x -= 8;
    } while(x >= 0);
    return true;
  }
  return false;
}

template <int EFFECT>
static inline void mode0RenderLine_Impl() {
  u16 *palette = (u16 *)paletteRAM;
  if(mode0CheckBlanking()) return;

  if(layerEnable & 0x0100) gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
  if(layerEnable & 0x0200) gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
  if(layerEnable & 0x0400) gfxDrawTextScreen(BG2CNT, BG2HOFS, BG2VOFS, line2);
  if(layerEnable & 0x0800) gfxDrawTextScreen(BG3CNT, BG3HOFS, BG3VOFS, line3);
  gfxDrawSprites(lineOBJ);

  u32 backdrop = (customBackdropColor == -1) ? (READ16LE(&palette[0]) | 0x30000000) : ((customBackdropColor & 0x7FFF) | 0x30000000);

  // Flatten state variables to raw pointers to map directly to GPRs
  // This allows the CPU to use 'lwzu' (Load Word and Update) instructions
  u32* l0 = line0; u32* l1 = line1; u32* l2 = line2; u32* l3 = line3;
  u32* lO = lineOBJ; u32* lM = lineMix;

  for(u32 x = 0; x < 240u; ++x) {
    u32 c0 = *l0++; u32 c1 = *l1++; u32 c2 = *l2++; u32 c3 = *l3++; u32 cO = *lO++;
    u32 color = backdrop;
    u8 top = 0x20;

    u8 li1 = (u8)(c1 >> 24);
    u8 li2 = (u8)(c2 >> 24);
    u8 li3 = (u8)(c3 >> 24);
    u8 li4 = (u8)(cO >> 24);

    // Pure branchless bitwise MIN mapping
    u8 r_12 = li2 ^ ((li1 ^ li2) & -(li1 < li2));
    u8 r_34 = li4 ^ ((li3 ^ li4) & -(li3 < li4));
    u8 r    = r_34 ^ ((r_12 ^ r_34) & -(r_12 < r_34));

    if(c0 < backdrop) {
      color = c0;
      top = 0x01;
    }

    if(r < (u8)(color >> 24)) {
      if(r == li1){ color = c1; top = 0x02; }
      else if(r == li2){ color = c2; top = 0x04; }
      else if(r == li3){ color = c3; top = 0x08; }
      else if(r == li4){ color = cO; top = 0x10; }
    }

    if((top & 0x10) && (color & 0x00010000)) {
      u32 back = backdrop;
      u8 top2 = 0x20;

      u8 li0 = (u8)(c0 >> 24);
      u8 r_01 = li1 ^ ((li0 ^ li1) & -(li0 < li1));
      u8 r_23 = li3 ^ ((li2 ^ li3) & -(li2 < li3));
      u8 r_back = r_23 ^ ((r_01 ^ r_23) & -(r_01 < r_23));

      if(r_back < (u8)(color >> 24)) {
        if(r_back == li0) { back = c0; top2 = 0x01; }
        else if(r_back == li1) { back = c1; top2 = 0x02; }
        else if(r_back == li2) { back = c2; top2 = 0x04; }
        else if(r_back == li3) { back = c3; top2 = 0x08; }
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
}

template <int EFFECT>
static inline void mode0RenderLineNoWindow_Impl() {
  u16 *palette = (u16 *)paletteRAM;
  if(mode0CheckBlanking()) return;

  if(layerEnable & 0x0100) gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
  if(layerEnable & 0x0200) gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
  if(layerEnable & 0x0400) gfxDrawTextScreen(BG2CNT, BG2HOFS, BG2VOFS, line2);
  if(layerEnable & 0x0800) gfxDrawTextScreen(BG3CNT, BG3HOFS, BG3VOFS, line3);
  gfxDrawSprites(lineOBJ);

  u32 backdrop = (customBackdropColor == -1) ? (READ16LE(&palette[0]) | 0x30000000) : ((customBackdropColor & 0x7FFF) | 0x30000000);

  // Flatten state variables to raw pointers
  u32* l0 = line0; u32* l1 = line1; u32* l2 = line2; u32* l3 = line3;
  u32* lO = lineOBJ; u32* lM = lineMix;

  for(int x = 0; x < 240; x++) {
    u32 color = backdrop;
    u8 top = 0x20;

    u32 c0 = *l0++; u32 c1 = *l1++; u32 c2 = *l2++; u32 c3 = *l3++; u32 cO = *lO++;

    u8 li1 = (u8)(c1>>24);
    u8 li2 = (u8)(c2>>24);
    u8 li3 = (u8)(c3>>24);
    u8 li4 = (u8)(cO>>24);

    u8 r_12 = li2 ^ ((li1 ^ li2) & -(li1 < li2));
    u8 r_34 = li4 ^ ((li3 ^ li4) & -(li3 < li4));
    u8 r    = r_34 ^ ((r_12 ^ r_34) & -(r_12 < r_34));

    if(c0 < backdrop) {
      color = c0;
      top = 0x01;
    }

    if(r < (u8)(color >> 24)) {
      if(r == li1) { color = c1; top = 0x02; }
      else if(r == li2) { color = c2; top = 0x04; }
      else if(r == li3) { color = c3; top = 0x08; }
      else if(r == li4) { color = cO; top = 0x10; }
    }

    if(!(color & 0x00010000)) {
      if (EFFECT == 1) {
        if(top & BLDMOD) {
          u32 back = backdrop;
          u8 top2 = 0x20;

          if((top != 0x01) && c0 < back) { back = c0; top2 = 0x01; }
          if((top != 0x02) && c1 < (back & 0xFF000000)) { back = c1; top2 = 0x02; }
          if((top != 0x04) && c2 < (back & 0xFF000000)) { back = c2; top2 = 0x04; }
          if((top != 0x08) && c3 < (back & 0xFF000000)) { back = c3; top2 = 0x08; }
          if((top != 0x10) && cO < (back & 0xFF000000)) { back = cO; top2 = 0x10; }

          if(top2 & (BLDMOD>>8))
            color = gfxAlphaBlend(color, back, coeff[COLEV & 0x1F], coeff[(COLEV >> 8) & 0x1F]);
        }
      } else if (EFFECT == 2) {
        if(BLDMOD & top) color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
      } else if (EFFECT == 3) {
        if(BLDMOD & top) color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
      }
    } else {
      u32 back = backdrop;
      u8 top2 = 0x20;

      if(c0 < back) { back = c0; top2 = 0x01; }
      if(c1 < (back & 0xFF000000)) { back = c1; top2 = 0x02; }
      if(c2 < (back & 0xFF000000)) { back = c2; top2 = 0x04; }
      if(c3 < (back & 0xFF000000)) { back = c3; top2 = 0x08; }

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
}

template <int EFFECT>
static inline void mode0RenderLineAll_Impl() {
  u16 *palette = (u16 *)paletteRAM;
  if(mode0CheckBlanking()) return;

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
  if(layerEnable & 0x0400) gfxDrawTextScreen(BG2CNT, BG2HOFS, BG2VOFS, line2);
  if(layerEnable & 0x0800) gfxDrawTextScreen(BG3CNT, BG3HOFS, BG3VOFS, line3);

  gfxDrawSprites(lineOBJ);
  gfxDrawOBJWin(lineOBJWin);

  u32 backdrop = (customBackdropColor == -1) ? (READ16LE(&palette[0]) | 0x30000000) : ((customBackdropColor & 0x7FFF) | 0x30000000);

  u8 inWin0Mask = WININ & 0xFF;
  u8 inWin1Mask = WININ >> 8;
  u8 outMask = WINOUT & 0xFF;

  // Flatten state variables to raw pointers
  u32* l0 = line0; u32* l1 = line1; u32* l2 = line2; u32* l3 = line3;
  u32* lO = lineOBJ; u32* lM = lineMix; u32* lW = lineOBJWin;
  const bool* w0 = gfxInWin0; const bool* w1 = gfxInWin1;

  for(int x = 0; x < 240; x++) {
    u32 color = backdrop;
    u8 top = 0x20;
    u8 mask = outMask;

    u32 c0 = *l0++; u32 c1 = *l1++; u32 c2 = *l2++; u32 c3 = *l3++; u32 cO = *lO++;
    u32 cW = *lW++;
    bool win0 = *w0++;
    bool win1 = *w1++;

    if(!(cW & 0x80000000)) mask = WINOUT >> 8;
    if(inWindow1 && win1) mask = inWin1Mask;
    if(inWindow0 && win0) mask = inWin0Mask;

    if((mask & 1) && (c0 < color)) { color = c0; top = 0x01; }
    if((mask & 2) && ((u8)(c1>>24) < (u8)(color >> 24))) { color = c1; top = 0x02; }
    if((mask & 4) && ((u8)(c2>>24) < (u8)(color >> 24))) { color = c2; top = 0x04; }
    if((mask & 8) && ((u8)(c3>>24) < (u8)(color >> 24))) { color = c3; top = 0x08; }
    if((mask & 16) && ((u8)(cO>>24) < (u8)(color >> 24))) { color = cO; top = 0x10; }

    if(color & 0x00010000) {
      u32 back = backdrop;
      u8 top2 = 0x20;

      if((mask & 1) && ((u8)(c0>>24) < (u8)(back >> 24))) { back = c0; top2 = 0x01; }
      if((mask & 2) && ((u8)(c1>>24) < (u8)(back >> 24))) { back = c1; top2 = 0x02; }
      if((mask & 4) && ((u8)(c2>>24) < (u8)(back >> 24))) { back = c2; top2 = 0x04; }
      if((mask & 8) && ((u8)(c3>>24) < (u8)(back >> 24))) { back = c3; top2 = 0x08; }

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

          if((mask & 1) && (top != 0x01) && (u8)(c0>>24) < (u8)(back >> 24)) { back = c0; top2 = 0x01; }
          if((mask & 2) && (top != 0x02) && (u8)(c1>>24) < (u8)(back >> 24)) { back = c1; top2 = 0x02; }
          if((mask & 4) && (top != 0x04) && (u8)(c2>>24) < (u8)(back >> 24)) { back = c2; top2 = 0x04; }
          if((mask & 8) && (top != 0x08) && (u8)(c3>>24) < (u8)(back >> 24)) { back = c3; top2 = 0x08; }
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
}

void mode0RenderLine() {
  switch((BLDMOD >> 6) & 3) {
    case 0: mode0RenderLine_Impl<0>(); break;
    case 1: mode0RenderLine_Impl<1>(); break;
    case 2: mode0RenderLine_Impl<2>(); break;
    case 3: mode0RenderLine_Impl<3>(); break;
  }
}

void mode0RenderLineNoWindow() {
  switch((BLDMOD >> 6) & 3) {
    case 0: mode0RenderLineNoWindow_Impl<0>(); break;
    case 1: mode0RenderLineNoWindow_Impl<1>(); break;
    case 2: mode0RenderLineNoWindow_Impl<2>(); break;
    case 3: mode0RenderLineNoWindow_Impl<3>(); break;
  }
}

void mode0RenderLineAll() {
  switch((BLDMOD >> 6) & 3) {
    case 0: mode0RenderLineAll_Impl<0>(); break;
    case 1: mode0RenderLineAll_Impl<1>(); break;
    case 2: mode0RenderLineAll_Impl<2>(); break;
    case 3: mode0RenderLineAll_Impl<3>(); break;
  }
}

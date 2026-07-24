#ifndef GFX_H
#define GFX_H

#include "../common/Port.h"

extern void gfxDrawTextScreen(u16, u16, u16, u32 *);

void gfxDrawRotScreen(u16,
			     u16, u16,
			     u16, u16,
			     u16, u16,
			     u16, u16,
			     int&, int&,
			     int,
			     u32*);
void gfxDrawRotScreen16Bit(u16,
				  u16, u16,
				  u16, u16,
				  u16, u16,
				  u16, u16,
				  int&, int&,
				  int,
				  u32*);
void gfxDrawRotScreen256(u16,
				u16, u16,
				u16, u16,
				u16, u16,
				u16, u16,
				int&, int&,
				int,
				u32*);
void gfxDrawRotScreen16Bit160(u16,
				     u16, u16,
				     u16, u16,
				     u16, u16,
				     u16, u16,
				     int&, int&,
				     int,
				     u32*);

void gfxDrawSprites(u32 *lineOBJ);
void gfxDrawOBJWin(u32 *lineOBJWin);
static void gfxIncreaseBrightness(u32 *line, int coeff);
static void gfxDecreaseBrightness(u32 *line, int coeff);
static void gfxAlphaBlend(u32 *ta, u32 *tb, int ca, int cb);

void mode0RenderLine();
void mode0RenderLineNoWindow();
void mode0RenderLineAll();

void mode1RenderLine();
void mode1RenderLineNoWindow();
void mode1RenderLineAll();

void mode2RenderLine();
void mode2RenderLineNoWindow();
void mode2RenderLineAll();

void mode3RenderLine();
void mode3RenderLineNoWindow();
void mode3RenderLineAll();

void mode4RenderLine();
void mode4RenderLineNoWindow();
void mode4RenderLineAll();

void mode5RenderLine();
void mode5RenderLineNoWindow();
void mode5RenderLineAll();

extern int coeff[32];
extern u32 line0[240];
extern u32 line1[240];
extern u32 line2[240];
extern u32 line3[240];
extern u32 lineOBJ[240];
extern u32 lineOBJWin[240];
extern u32 lineMix[240];
extern bool gfxInWin0[240];
extern bool gfxInWin1[240];
extern int lineOBJpixleft[128];

extern int gfxBG2Changed;
extern int gfxBG3Changed;

extern int gfxBG2X;
extern int gfxBG2Y;
extern int gfxBG3X;
extern int gfxBG3Y;
extern int gfxLastVCOUNT;

static inline u32 gfxIncreaseBrightness(u32 color, int coeff)
{
  color &= 0xffff;
  color = ((color << 16) | color) & 0x3E07C1F;

  color = color + (((0x3E07C1F - color) * coeff) >> 4);
  color &= 0x3E07C1F;

  return (color >> 16) | color;
}

static inline void gfxIncreaseBrightness(u32 *line, int coeff)
{
  for(int x = 0; x < 240; x++) {
    u32 color = *line;

    // Isolate the 16-bit pixel first to prevent high-word data bleed
    // from destroying the SWAR Green channel.
    u32 pixel = color & 0xFFFF;
    u32 c1 = ((pixel << 16) | pixel) & 0x03E07C1F;

    // Process all 3 channels in parallel
    u32 blended = c1 + (((0x03E07C1F - c1) * coeff) >> 4);
    blended &= 0x03E07C1F;

    // Repack and assign (preserving the original top 16 bits)
    *line++ = (color & 0xFFFF0000) | (blended >> 16) | blended;
  }
}

static inline u32 gfxDecreaseBrightness(u32 color, int coeff)
{
  color &= 0xffff;
  color = ((color << 16) | color) & 0x3E07C1F;

  color = color - (((color * coeff) >> 4) & 0x3E07C1F);

  return (color >> 16) | color;
}

static inline void gfxDecreaseBrightness(u32 *line, int coeff)
{
  for(int x = 0; x < 240; x++) {
    u32 color = *line;

    // Isolate the 16-bit pixel first
    u32 pixel = color & 0xFFFF;
    u32 c1 = ((pixel << 16) | pixel) & 0x03E07C1F;

    // Process all 3 channels in parallel
    u32 blended = c1 - (((c1 * coeff) >> 4) & 0x03E07C1F);

    // Repack and assign
    *line++ = (color & 0xFFFF0000) | (blended >> 16) | blended;
  }
}

static inline u32 gfxAlphaBlend(u32 color, u32 color2, int ca, int cb)
{
  if(color < 0x80000000) {
    // Mask out upper bits before SWAR unpacking to prevent data bleed
    color &= 0xffff;
    color2 &= 0xffff;

    // SWAR: Unpack and align channels
    u32 c1 = ((color << 16) | color) & 0x03E07C1F;
    u32 c2 = ((color2 << 16) | color2) & 0x03E07C1F;

    // Unified 32-bit mask multiplier for R, G, B concurrently
    u32 blended = ((c1 * ca) + (c2 * cb)) >> 4;

    // Only apply the branchless SWAR clamp when mathematically needed
    if ((ca + cb) > 16) {
      // Branchless SWAR clamping
      u32 overflow = blended & 0x04008020;
      u32 clamp_mask = overflow - (overflow >> 5);
      blended |= clamp_mask;
    }

    blended &= 0x03E07C1F;
    color = (blended >> 16) | blended;
  }
  return color;
}

static inline void gfxAlphaBlend(u32 *ta, u32 *tb, int ca, int cb)
{
  // Hoist the loop-invariant clamp condition to save cycles
  bool do_clamp = (ca + cb) > 16;

  // Flattened for GCC auto-vectorization and dual-issue ALU execution
  for(int x = 0; x < 240; x++) {
    u32 color = *ta;
    if(color < 0x80000000) {
      u32 color2 = *tb;

      u32 c1 = ((color << 16) | color) & 0x03E07C1F;
      u32 c2 = ((color2 << 16) | color2) & 0x03E07C1F;

      u32 blended = ((c1 * ca) + (c2 * cb)) >> 4;

      if (do_clamp) {
        u32 overflow = blended & 0x04008020;
        u32 clamp_mask = overflow - (overflow >> 5);
        blended |= clamp_mask;
      }

      blended &= 0x03E07C1F;
      *ta = (color & 0xFFFF0000) | (blended >> 16) | blended;
    }
    ta++;
    tb++;
  }
}

#endif // GFX_H

#include <string.h>
#include "GBA.h"
#include "Globals.h"
#include "../System.h"
#include "../common/Port.h"

int coeff[32] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};

u32 line0[240];
u32 line1[240];
u32 line2[240];
u32 line3[240];
u32 lineOBJ[240];
u32 lineOBJWin[240];
u32 lineMix[240];
bool gfxInWin0[240];
bool gfxInWin1[240];
int lineOBJpixleft[128];

int gfxBG2Changed = 0;
int gfxBG3Changed = 0;

int gfxBG2X = 0;
int gfxBG2Y = 0;
int gfxBG3X = 0;
int gfxBG3Y = 0;
int gfxLastVCOUNT = 0;

static inline void gfxClearArray(u32 *array)
{
  for(int i = 0; i < 240; i++) {
    *array++ = 0x80000000;
  }
}

union TileEntry
{
   struct
   {
      /*14*/	unsigned palette:4;
      /*13*/	unsigned vFlip:1;
      /*12*/	unsigned hFlip:1;
      /* 0*/	unsigned tileNum:10;
   };
   u16 val;
};

struct TileLine
{
   u32 pixels[8];
};

typedef const TileLine (*TileReader) (const u16 *, const int, const u8 *, u16 *, const u32);

static inline void gfxDrawPixel(u32 *dest, const u8 color, const u16 *palette, const u32 prio)
{
   // Pure branchless bitwise clamping mapping to 1-cycle execution
   s32 c = color;
   u32 mask = (c | -c) >> 31;
   *dest = ((READ16LE(&palette[color]) | prio) & mask) | (0x80000000 & ~mask);
}

inline const TileLine gfxReadTile(const u16 *screenSource, const int yyy, const u8 *charBase, u16 *palette, const u32 prio)
{
   TileEntry tile;
   tile.val = READ16LE(screenSource);

   int tileY = yyy & 7;
   if (tile.vFlip) tileY = 7 - tileY;
   TileLine tileLine;

   const u8 *tileBase = &charBase[tile.tileNum * 64 + tileY * 8];

   if (!tile.hFlip)
   {
      gfxDrawPixel(&tileLine.pixels[0], tileBase[0], palette, prio);
      gfxDrawPixel(&tileLine.pixels[1], tileBase[1], palette, prio);
      gfxDrawPixel(&tileLine.pixels[2], tileBase[2], palette, prio);
      gfxDrawPixel(&tileLine.pixels[3], tileBase[3], palette, prio);
      gfxDrawPixel(&tileLine.pixels[4], tileBase[4], palette, prio);
      gfxDrawPixel(&tileLine.pixels[5], tileBase[5], palette, prio);
      gfxDrawPixel(&tileLine.pixels[6], tileBase[6], palette, prio);
      gfxDrawPixel(&tileLine.pixels[7], tileBase[7], palette, prio);
   }
   else
   {
      gfxDrawPixel(&tileLine.pixels[0], tileBase[7], palette, prio);
      gfxDrawPixel(&tileLine.pixels[1], tileBase[6], palette, prio);
      gfxDrawPixel(&tileLine.pixels[2], tileBase[5], palette, prio);
      gfxDrawPixel(&tileLine.pixels[3], tileBase[4], palette, prio);
      gfxDrawPixel(&tileLine.pixels[4], tileBase[3], palette, prio);
      gfxDrawPixel(&tileLine.pixels[5], tileBase[2], palette, prio);
      gfxDrawPixel(&tileLine.pixels[6], tileBase[1], palette, prio);
      gfxDrawPixel(&tileLine.pixels[7], tileBase[0], palette, prio);
   }

   return tileLine;
}

inline const TileLine gfxReadTilePal(const u16 *screenSource, const int yyy, const u8 *charBase, u16 *palette, const u32 prio)
{
   TileEntry tile;
   tile.val = READ16LE(screenSource);

   int tileY = yyy & 7;
   if (tile.vFlip) tileY = 7 - tileY;
   palette += tile.palette * 16;
   TileLine tileLine;

   const u8 *tileBase = &charBase[tile.tileNum * 32 + tileY * 4];

   if (!tile.hFlip)
   {
      gfxDrawPixel(&tileLine.pixels[0], tileBase[0] & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[1], tileBase[0] >> 4,   palette, prio);
      gfxDrawPixel(&tileLine.pixels[2], tileBase[1] & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[3], tileBase[1] >> 4,   palette, prio);
      gfxDrawPixel(&tileLine.pixels[4], tileBase[2] & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[5], tileBase[2] >> 4,   palette, prio);
      gfxDrawPixel(&tileLine.pixels[6], tileBase[3] & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[7], tileBase[3] >> 4,   palette, prio);
   }
   else
   {
      gfxDrawPixel(&tileLine.pixels[0], tileBase[3] >> 4,   palette, prio);
      gfxDrawPixel(&tileLine.pixels[1], tileBase[3] & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[2], tileBase[2] >> 4,   palette, prio);
      gfxDrawPixel(&tileLine.pixels[3], tileBase[2] & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[4], tileBase[1] >> 4,   palette, prio);
      gfxDrawPixel(&tileLine.pixels[5], tileBase[1] & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[6], tileBase[0] >> 4,   palette, prio);
      gfxDrawPixel(&tileLine.pixels[7], tileBase[0] & 0x0F, palette, prio);
   }

   return tileLine;
}

static inline void gfxDrawTile(const TileLine &tileLine, u32 *line)
{
   memcpy(line, tileLine.pixels, sizeof(tileLine.pixels));
}

static inline void gfxDrawTileClipped(const TileLine &tileLine, u32 *line, const int start, int w)
{
   memcpy(line, tileLine.pixels + start, w * sizeof(u32));
}

template<TileReader readTile>
static void gfxDrawTextScreen(u16 control, u16 hofs, u16 vofs,
                       u32 *line)
{
   u16 *palette = (u16 *)paletteRAM;
   u8 *charBase = &vram[((control >> 2) & 0x03) * 0x4000];
   u16 *screenBase = (u16 *)&vram[((control >> 8) & 0x1f) * 0x800];
   u32 prio = ((control & 3)<<25) + 0x1000000;
   int sizeX = 256;
   int sizeY = 256;
   switch ((control >> 14) & 3)
   {
      case 0:
         break;
      case 1:
         sizeX = 512;
         break;
      case 2:
         sizeY = 512;
         break;
      case 3:
         sizeX = 512;
         sizeY = 512;
         break;
   }

   int maskX = sizeX-1;
   int maskY = sizeY-1;

   bool mosaicOn = (control & 0x40) ? true : false;

   int xxx = hofs & maskX;
   int yyy = (vofs + VCOUNT) & maskY;
   int mosaicX = (MOSAIC & 0x000F)+1;
   int mosaicY = ((MOSAIC & 0x00F0)>>4)+1;

   if (mosaicOn)
   {
      if ((VCOUNT % mosaicY) != 0)
      {
         mosaicY = VCOUNT - (VCOUNT % mosaicY);
         yyy = (vofs + mosaicY) & maskY;
      }
   }

   if (yyy > 255 && sizeY > 256)
   {
      yyy &= 255;
      screenBase += 0x400;
      if (sizeX > 256)
         screenBase += 0x400;
   }

   int yshift = ((yyy>>3)<<5);

   u16 *screenSource = screenBase + 0x400 * (xxx>>8) + ((xxx & 255)>>3) + yshift;
   int x = 0;
   const int firstTileX = xxx & 7;

   // First tile, if clipped
   if (firstTileX)
   {
      gfxDrawTileClipped(readTile(screenSource, yyy, charBase, palette, prio), &line[x], firstTileX, 8 - firstTileX);
      screenSource++;
      x += 8 - firstTileX;
      xxx += 8 - firstTileX;

      if (xxx == 256 && sizeX > 256)
      {
         screenSource = screenBase + 0x400 + yshift;
      }
      else if (xxx >= sizeX)
      {
         xxx = 0;
         screenSource = screenBase + yshift;
      }
   }

   // Middle tiles, full
   while (x < 240 - firstTileX)
   {
	  // PREFETCH: Pull the next tile's memory into L1 D-Cache asynchronously
	  __builtin_prefetch(screenSource + 4, 0, 0);

      gfxDrawTile(readTile(screenSource, yyy, charBase, palette, prio), &line[x]);
      screenSource++;
      xxx += 8;
      x += 8;

      if (xxx == 256 && sizeX > 256)
      {
         screenSource = screenBase + 0x400 + yshift;
      }
      else if (xxx >= sizeX)
      {
         xxx = 0;
         screenSource = screenBase + yshift;
      }
   }

   // Last tile, if clipped
   if (firstTileX)
   {
      gfxDrawTileClipped(readTile(screenSource, yyy, charBase, palette, prio), &line[x], 0, firstTileX);
   }

	if (mosaicOn)
	{
		if (mosaicX > 1)
		{
			for (int i = 0; i < 240; i += mosaicX)
			{
				u32 pixel = line[i];
				int limit = i + mosaicX;

				// Mathematical clamp without branching out of the loop
				limit = (limit > 240) ? 240 : limit;

				for (int m = i + 1; m < limit; m++)
				{
					line[m] = pixel;
				}
			}
		}
	}
}

void gfxDrawTextScreen(u16 control, u16 hofs, u16 vofs, u32 *line)
{
   if (control & 0x80) // 1 pal / 256 col
      gfxDrawTextScreen<gfxReadTile>(control, hofs, vofs, line);
   else // 16 pal / 16 col
      gfxDrawTextScreen<gfxReadTilePal>(control, hofs, vofs, line);
}

void gfxDrawRotScreen(u16 control,
				    u16 x_l, u16 x_h,
				    u16 y_l, u16 y_h,
				    u16 pa,  u16 pb,
				    u16 pc,  u16 pd,
				    int& currentX, int& currentY,
				    int changed,
				    u32 *line)
{
  u16 *palette = (u16 *)paletteRAM;
  u8 *charBase = &vram[((control >> 2) & 0x03) * 0x4000];
  u8 *screenBase = (u8 *)&vram[((control >> 8) & 0x1f) * 0x800];
  int prio = ((control & 3) << 25) + 0x1000000;

  int sizeX = 128;
  int sizeY = 128;
  switch((control >> 14) & 3) {
  case 0:
    break;
  case 1:
    sizeX = sizeY = 256;
    break;
  case 2:
    sizeX = sizeY = 512;
    break;
  case 3:
    sizeX = sizeY = 1024;
    break;
  }

  int maskX = sizeX-1;
  int maskY = sizeY-1;

  int yshift = ((control >> 14) & 3)+4;

  int dx = pa & 0x7FFF;
  if(pa & 0x8000)
    dx |= 0xFFFF8000;
  int dmx = pb & 0x7FFF;
  if(pb & 0x8000)
    dmx |= 0xFFFF8000;
  int dy = pc & 0x7FFF;
  if(pc & 0x8000)
    dy |= 0xFFFF8000;
  int dmy = pd & 0x7FFF;
  if(pd & 0x8000)
    dmy |= 0xFFFF8000;

  if(VCOUNT == 0)
    changed = 3;

  if(changed & 1) {
    currentX = (x_l) | ((x_h & 0x07FF)<<16);
    if(x_h & 0x0800)
      currentX |= 0xF8000000;
  } else {
    currentX += dmx;
  }

  if(changed & 2) {
    currentY = (y_l) | ((y_h & 0x07FF)<<16);
    if(y_h & 0x0800)
      currentY |= 0xF8000000;
  } else {
    currentY += dmy;
  }

  int realX = currentX;
  int realY = currentY;

  if(control & 0x40) {
    int mosaicY = ((MOSAIC & 0xF0)>>4) + 1;
    int y = (VCOUNT % mosaicY);
    realX -= y*dmx;
    realY -= y*dmy;
  }

  if(control & 0x2000) {
    for(int x = 0; x < 240; x++) {
      int xxx = (realX >> 8) & maskX;
      int yyy = (realY >> 8) & maskY;

      int tile = screenBase[(xxx>>3) + ((yyy>>3)<<yshift)];

      int tileX = (xxx & 7);
      int tileY = yyy & 7;

      u8 color = charBase[(tile<<6) + (tileY<<3) + tileX];

      u32 mask = -(color != 0);
	  line[x] = ((READ16LE(&palette[color]) | prio) & mask) | (0x80000000 & ~mask);

      realX += dx;
      realY += dy;
    }
  } else {
    for(int x = 0; x < 240; x++) {
      int xxx = (realX >> 8);
      int yyy = (realY >> 8);

      if(xxx < 0 ||
         yyy < 0 ||
         xxx >= sizeX ||
         yyy >= sizeY) {
        line[x] = 0x80000000;
      } else {
        int tile = screenBase[(xxx>>3) + ((yyy>>3)<<yshift)];

        int tileX = (xxx & 7);
        int tileY = yyy & 7;

        u8 color = charBase[(tile<<6) + (tileY<<3) + tileX];

        u32 mask = -(color != 0);
		line[x] = ((READ16LE(&palette[color]) | prio) & mask) | (0x80000000 & ~mask);
      }
      realX += dx;
      realY += dy;
    }
  }

  if(control & 0x40) {
    int mosaicX = (MOSAIC & 0xF) + 1;
    if(mosaicX > 1) {
      int m = 1;
      for(int i = 0; i < 239; i++) {
        line[i+1] = line[i];
        m++;
        if(m == mosaicX) {
          m = 1;
          i++;
        }
      }
    }
  }
}

void gfxDrawRotScreen16Bit(u16 control,
					 u16 x_l, u16 x_h,
					 u16 y_l, u16 y_h,
					 u16 pa,  u16 pb,
					 u16 pc,  u16 pd,
					 int& currentX, int& currentY,
					 int changed,
					 u32 *line)
{
  u16 *screenBase = (u16 *)&vram[0];
  int prio = ((control & 3) << 25) + 0x1000000;
  int sizeX = 240;
  int sizeY = 160;

  int startX = (x_l) | ((x_h & 0x07FF)<<16);
  if(x_h & 0x0800)
    startX |= 0xF8000000;
  int startY = (y_l) | ((y_h & 0x07FF)<<16);
  if(y_h & 0x0800)
    startY |= 0xF8000000;

  int dx = pa & 0x7FFF;
  if(pa & 0x8000)
    dx |= 0xFFFF8000;
  int dmx = pb & 0x7FFF;
  if(pb & 0x8000)
    dmx |= 0xFFFF8000;
  int dy = pc & 0x7FFF;
  if(pc & 0x8000)
    dy |= 0xFFFF8000;
  int dmy = pd & 0x7FFF;
  if(pd & 0x8000)
    dmy |= 0xFFFF8000;

  if(VCOUNT == 0)
    changed = 3;

  if(changed & 1) {
    currentX = (x_l) | ((x_h & 0x07FF)<<16);
    if(x_h & 0x0800)
      currentX |= 0xF8000000;
  } else
    currentX += dmx;

  if(changed & 2) {
    currentY = (y_l) | ((y_h & 0x07FF)<<16);
    if(y_h & 0x0800)
      currentY |= 0xF8000000;
  } else {
    currentY += dmy;
  }

  int realX = currentX;
  int realY = currentY;

  if(control & 0x40) {
    int mosaicY = ((MOSAIC & 0xF0)>>4) + 1;
    int y = (VCOUNT % mosaicY);
    realX -= y*dmx;
    realY -= y*dmy;
  }

  int xxx = (realX >> 8);
  int yyy = (realY >> 8);

  for(int x = 0; x < 240; x++) {
    if(xxx < 0 ||
       yyy < 0 ||
       xxx >= sizeX ||
       yyy >= sizeY) {
      line[x] = 0x80000000;
    } else {
      line[x] = (READ16LE(&screenBase[yyy * sizeX + xxx]) | prio);
    }
    realX += dx;
    realY += dy;

    xxx = (realX >> 8);
    yyy = (realY >> 8);
  }

  if(control & 0x40) {
    int mosaicX = (MOSAIC & 0xF) + 1;
    if(mosaicX > 1) {
      int m = 1;
      for(int i = 0; i < 239; i++) {
        line[i+1] = line[i];
        m++;
        if(m == mosaicX) {
          m = 1;
          i++;
        }
      }
    }
  }
}

void gfxDrawRotScreen256(u16 control,
				       u16 x_l, u16 x_h,
				       u16 y_l, u16 y_h,
				       u16 pa,  u16 pb,
				       u16 pc,  u16 pd,
				       int &currentX, int& currentY,
				       int changed,
				       u32 *line)
{
  u16 *palette = (u16 *)paletteRAM;

  // OPTIMIZATION: Ternary replaced with predictable branch
  u8 *screenBase = &vram[0x0000];
  if(DISPCNT & 0x0010) {
      screenBase = &vram[0xA000];
  }

  int prio = ((control & 3) << 25) + 0x1000000;
  int sizeX = 240;
  int sizeY = 160;

  int startX = (x_l) | ((x_h & 0x07FF)<<16);
  if(x_h & 0x0800) startX |= 0xF8000000;
  int startY = (y_l) | ((y_h & 0x07FF)<<16);
  if(y_h & 0x0800) startY |= 0xF8000000;

  int dx = pa & 0x7FFF;
  if(pa & 0x8000) dx |= 0xFFFF8000;
  int dmx = pb & 0x7FFF;
  if(pb & 0x8000) dmx |= 0xFFFF8000;
  int dy = pc & 0x7FFF;
  if(pc & 0x8000) dy |= 0xFFFF8000;
  int dmy = pd & 0x7FFF;
  if(pd & 0x8000) dmy |= 0xFFFF8000;

  if(VCOUNT == 0) changed = 3;

  if(changed & 1) {
    currentX = (x_l) | ((x_h & 0x07FF)<<16);
    if(x_h & 0x0800) currentX |= 0xF8000000;
  } else {
    currentX += dmx;
  }

  if(changed & 2) {
    currentY = (y_l) | ((y_h & 0x07FF)<<16);
    if(y_h & 0x0800) currentY |= 0xF8000000;
  } else {
    currentY += dmy;
  }

  int realX = currentX;
  int realY = currentY;

  if(control & 0x40) {
    int mosaicY = ((MOSAIC & 0xF0)>>4) + 1;
    int y = VCOUNT;

    // OPTIMIZATION: Avoid 30-cycle divw penalty if mosaicY is 1 (Standard rendering)
    if (mosaicY > 1) {
        y -= (VCOUNT % mosaicY);
    }

    realX = startX + y*dmx;
    realY = startY + y*dmy;
  }

  int xxx = (realX >> 8);
  int yyy = (realY >> 8);

  for(int x = 0; x < 240; x++) {
    if(xxx < 0 || yyy < 0 || xxx >= sizeX || yyy >= sizeY) {
      line[x] = 0x80000000;
    } else {
      u8 color = screenBase[yyy * 240 + xxx];
      u32 mask = -(color != 0);
      line[x] = ((READ16LE(&palette[color]) | prio) & mask) | (0x80000000 & ~mask);
    }
    realX += dx;
    realY += dy;

    xxx = (realX >> 8);
    yyy = (realY >> 8);
  }

  if(control & 0x40) {
    int mosaicX = (MOSAIC & 0xF) + 1;
    if(mosaicX > 1) {
      int m = 1;
      for(int i = 0; i < 239; i++) {
        line[i+1] = line[i];
        m++;
        if(m == mosaicX) {
          m = 1;
          i++;
        }
      }
    }
  }
}

void gfxDrawRotScreen16Bit160(u16 control,
					    u16 x_l, u16 x_h,
					    u16 y_l, u16 y_h,
					    u16 pa,  u16 pb,
					    u16 pc,  u16 pd,
					    int& currentX, int& currentY,
					    int changed,
					    u32 *line)
{
  u16 *screenBase = (DISPCNT & 0x0010) ? (u16 *)&vram[0xa000] :
    (u16 *)&vram[0];
  int prio = ((control & 3) << 25) + 0x1000000;
  int sizeX = 160;
  int sizeY = 128;

  int startX = (x_l) | ((x_h & 0x07FF)<<16);
  if(x_h & 0x0800)
    startX |= 0xF8000000;
  int startY = (y_l) | ((y_h & 0x07FF)<<16);
  if(y_h & 0x0800)
    startY |= 0xF8000000;

  int dx = pa & 0x7FFF;
  if(pa & 0x8000)
    dx |= 0xFFFF8000;
  int dmx = pb & 0x7FFF;
  if(pb & 0x8000)
    dmx |= 0xFFFF8000;
  int dy = pc & 0x7FFF;
  if(pc & 0x8000)
    dy |= 0xFFFF8000;
  int dmy = pd & 0x7FFF;
  if(pd & 0x8000)
    dmy |= 0xFFFF8000;

  if(VCOUNT == 0)
    changed = 3;

  if(changed & 1) {
    currentX = (x_l) | ((x_h & 0x07FF)<<16);
    if(x_h & 0x0800)
      currentX |= 0xF8000000;
  } else {
    currentX += dmx;
  }

  if(changed & 2) {
    currentY = (y_l) | ((y_h & 0x07FF)<<16);
    if(y_h & 0x0800)
      currentY |= 0xF8000000;
  } else {
    currentY += dmy;
  }

  int realX = currentX;
  int realY = currentY;

  if(control & 0x40) {
    int mosaicY = ((MOSAIC & 0xF0)>>4) + 1;
    int y = VCOUNT - (VCOUNT % mosaicY);
    realX = startX + y*dmx;
    realY = startY + y*dmy;
  }

  int xxx = (realX >> 8);
  int yyy = (realY >> 8);

  for(int x = 0; x < 240; x++) {
    if(xxx < 0 ||
       yyy < 0 ||
       xxx >= sizeX ||
       yyy >= sizeY) {
      line[x] = 0x80000000;
    } else {
      line[x] = (READ16LE(&screenBase[yyy * sizeX + xxx]) | prio);
    }
    realX += dx;
    realY += dy;

    xxx = (realX >> 8);
    yyy = (realY >> 8);
  }

  if(control & 0x40) {
    int mosaicX = (MOSAIC & 0xF) + 1;
    if(mosaicX > 1) {
      int m = 1;
      for(int i = 0; i < 239; i++) {
        line[i+1] = line[i];
        m++;
        if(m == mosaicX) {
          m = 1;
          i++;
        }
      }
    }
  }
}

void gfxDrawSprites(u32 *lineOBJ)
{
    // OPTIMIZATION: Removed Ternary to prevent branch penalty
	int lineOBJpix = 1226;
    if (DISPCNT & 0x20) {
        lineOBJpix = 954;
    }

	int m = 0;
	gfxClearArray(lineOBJ);
	if(!(layerEnable & 0x1000)) return;

	u16 *sprites = (u16 *)oam;
	u16 *spritePalette = &((u16 *)paletteRAM)[256];
	int mosaicY = ((MOSAIC & 0xF000)>>12) + 1;
	int mosaicX = ((MOSAIC & 0xF00)>>8) + 1;

	for(int x = 0; x < 128 ; x++) {
		// PREFETCH: Pull the next sprite's OAM chunk into the L1 cache
		__builtin_prefetch(sprites + 4, 0, 0);

		u16 a0 = READ16LE(sprites++);
		u16 a1 = READ16LE(sprites++);
		u16 a2 = READ16LE(sprites++);
		sprites++;

		lineOBJpixleft[x] = lineOBJpix;
		lineOBJpix -= 2;
		if (lineOBJpix <= 0) continue;

		if ((a0 & 0x0c00) == 0x0c00) a0 &= 0xF3FF;
		if ((a0 >> 14) == 3) {
			a0 &= 0x3FFF;
			a1 &= 0x3FFF;
		}

		int sizeX = 8 << (a1 >> 14);
		int sizeY = sizeX;

		if ((a0 >> 14) & 1) {
			if (sizeX < 32) sizeX <<= 1;
			if (sizeY > 8) sizeY >>= 1;
		}
		else if ((a0 >> 14) & 2) {
			if (sizeX > 8) sizeX >>= 1;
			if (sizeY < 32) sizeY <<= 1;
		}

		int sy = (a0 & 255);
		int sx = (a1 & 0x1FF);

		// OBJ-WIN checks untouched for logic parity
		if (((a0 & 0x0c00) == 0x0800) && (layerEnable & 0x8000)) {
			if ((a0 & 0x0300) == 0x0300) { sizeX <<= 1; sizeY <<= 1; }
			sy -= ((sy + sizeY) >> 8) << 8; // Branchless wrap

			if ((sx+sizeX)> 512) sx -= 512;
			if (sx < 0) { sizeX += sx; sx = 0; }
			else if ((sx+sizeX)> 240) sizeX = 240 - sx;

			if ((VCOUNT >= sy) && (VCOUNT < sy + sizeY) && (sx < 240)) {
				lineOBJpix -= (a0 & 0x0100) ? (8 + 2 * sizeX) : (sizeX - 2);
			}
			continue;
		}
		else if(((a0 & 0x0c00) == 0x0800) || ((a0 & 0x0300) == 0x0200)) {
			continue;
		}

		if (lineOBJpix < 0) continue;

        if(a0 & 0x0100) {
			int fieldX = sizeX;
			int fieldY = sizeY;
			if(a0 & 0x0200) {
			    fieldX <<= 1;
			    fieldY <<= 1;
			}

			// Branchless Y boundary wrapper
			sy -= ((sy + fieldY) >> 8) << 8;

			int t = VCOUNT - sy;
			if((t >= 0) && (t < fieldY)) {

                // OPTIMIZATION: Eliminated integer multiplication (mullw)
                // Replaced 'int startpix = ((sx + fieldX) >> 9) * (512 - sx);'
                int startpix = 0;
                if (sx + fieldX > 512) {
                    startpix = 512 - sx;
                }

                if (lineOBJpix > 0 && ((sx < 240) || startpix)) {
                    lineOBJpix -= 8;
                    int rot = (a1 >> 9) & 0x1F;
                    u16 *OAM = (u16 *)oam;

			  // Matrix prefetching
                    __builtin_prefetch(&OAM[3 + (rot << 4)], 0, 0);

                    int dx = READ16LE(&OAM[3 + (rot << 4)]);
                    if(dx & 0x8000) dx |= 0xFFFF8000;
                    int dmx = READ16LE(&OAM[7 + (rot << 4)]);
                    if(dmx & 0x8000) dmx |= 0xFFFF8000;
                    int dy = READ16LE(&OAM[11 + (rot << 4)]);
                    if(dy & 0x8000) dy |= 0xFFFF8000;
                    int dmy = READ16LE(&OAM[15 + (rot << 4)]);
                    if(dmy & 0x8000) dmy |= 0xFFFF8000;

                    // OPTIMIZATION: Bypassed 30-cycle 'divw' if mosaicY is 1
                    if(a0 & 0x1000) {
                        if (mosaicY > 1) {
                            t -= (t % mosaicY);
                        }
                    }

                    int realX = ((sizeX) << 7) - (fieldX >> 1)*dx - (fieldY>>1)*dmx + t * dmx;
                    int realY = ((sizeY) << 7) - (fieldX >> 1)*dy - (fieldY>>1)*dmy + t * dmy;

                    u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);

				if(a0 & 0x2000) {
				  int c = (a2 & 0x3FF);
				  if((DISPCNT & 7) > 2 && (c < 512))
					continue;
				  int inc = 32;
				  if(DISPCNT & 0x40)
					inc = sizeX >> 2;
				  else
					c &= 0x3FE;
				  for(int x = 0; x < fieldX; x++) {
					if (x >= startpix)
					  lineOBJpix-=2;
					if (lineOBJpix<0)
					  continue;
					int xxx = realX >> 8;
					int yyy = realY >> 8;

					if(xxx < 0 || xxx >= sizeX ||
					   yyy < 0 || yyy >= sizeY ||
					   sx >= 240);
					else {
					  u32 color = vram[0x10000 + ((((c + (yyy>>3) * inc)<<5)
										+ ((yyy & 7)<<3) + ((xxx >> 3)<<6) +
										(xxx & 7))&0x7FFF)];
					  if ((color==0) && (((prio >> 25)&3) <
										 ((lineOBJ[sx]>>25)&3))) {
						lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
						if((a0 & 0x1000) && m)
						  lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
					  } else if((color) && (prio < (lineOBJ[sx]&0xFF000000))) {
						lineOBJ[sx] = READ16LE(&spritePalette[color]) | prio;
						if((a0 & 0x1000) && m)
						  lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
					  }

					  if (a0 & 0x1000) {
						m++;
						if (m==mosaicX)
						  m=0;
					  }
					}
					sx = (sx+1)&511;
					realX += dx;
					realY += dy;
				  }
				} else {
				  int c = (a2 & 0x3FF);
				  if((DISPCNT & 7) > 2 && (c < 512))
					continue;

				  int inc = 32;
				  if(DISPCNT & 0x40)
					inc = sizeX >> 3;
				  int palette = (a2 >> 8) & 0xF0;
				  for(int x = 0; x < fieldX; x++) {
					if (x >= startpix)
					  lineOBJpix-=2;
					if (lineOBJpix<0)
					  continue;
					int xxx = realX >> 8;
					int yyy = realY >> 8;
					if(xxx < 0 || xxx >= sizeX ||
					   yyy < 0 || yyy >= sizeY ||
					   sx >= 240);
					else {
					  u32 color = vram[0x10000 + ((((c + (yyy>>3) * inc)<<5)
													+ ((yyy & 7)<<2) + ((xxx >> 3)<<5) +
												   ((xxx & 7)>>1))&0x7FFF)];
					  if(xxx & 1)
						color >>= 4;
					  else
						color &= 0x0F;

					  if ((color==0) && (((prio >> 25)&3) <
										 ((lineOBJ[sx]>>25)&3))) {
						lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
						if((a0 & 0x1000) && m)
						  lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
					  } else if((color) && (prio < (lineOBJ[sx]&0xFF000000))) {
						lineOBJ[sx] = READ16LE(&spritePalette[palette+color]) | prio;
						if((a0 & 0x1000) && m)
						  lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
					  }
					}
					if((a0 & 0x1000) && m) {
					  m++;
					  if (m==mosaicX)
						m=0;
					}

					sx = (sx+1)&511;
					realX += dx;
					realY += dy;

				  }
				}
			  }
			}
		}
		else {
			if(sy+sizeY > 256)
			  sy -= 256;
			int t = VCOUNT - sy;
			if((t >= 0) && (t < sizeY)) {
			  int startpix = 0;
			  if ((sx+sizeX)> 512)
			  {
				startpix=512-sx;
			  }
			  if((sx < 240) || startpix) {
				lineOBJpix+=2;
				if(a0 & 0x2000) {
				  if(a1 & 0x2000)
					t = sizeY - t - 1;
				  int c = (a2 & 0x3FF);
				  if((DISPCNT & 7) > 2 && (c < 512))
					continue;

				  int inc = 32;
				  if(DISPCNT & 0x40) {
					inc = sizeX >> 2;
				  } else {
					c &= 0x3FE;
				  }
				  int xxx = 0;
				  if(a1 & 0x1000)
					xxx = sizeX-1;

				  if(a0 & 0x1000) {
					t -= (t % mosaicY);
				  }

				  int address = 0x10000 + ((((c+ (t>>3) * inc) << 5)
					+ ((t & 7) << 3) + ((xxx>>3)<<6) + (xxx & 7)) & 0x7FFF);

				  if(a1 & 0x1000)
					xxx = 7;
				  u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);

				  for(int xx = 0; xx < sizeX; xx++) {
					if (xx >= startpix)
					  lineOBJpix--;
					if (lineOBJpix<0)
					  continue;
					if(sx < 240) {
					  u8 color = vram[address];
					  if ((color==0) && (((prio >> 25)&3) <
										 ((lineOBJ[sx]>>25)&3))) {
						lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
						if((a0 & 0x1000) && m)
						  lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
					  } else if((color) && (prio < (lineOBJ[sx]&0xFF000000))) {
						lineOBJ[sx] = READ16LE(&spritePalette[color]) | prio;
						if((a0 & 0x1000) && m)
						  lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
					  }

					  if (a0 & 0x1000) {
						m++;
						if (m==mosaicX)
						  m=0;
					  }

					}

					sx = (sx+1) & 511;
					if(a1 & 0x1000) {
					  xxx--;
					  address--;
					  if(xxx == -1) {
						address -= 56;
						xxx = 7;
					  }
					  if(address < 0x10000)
						address += 0x8000;
					} else {
					  xxx++;
					  address++;
					  if(xxx == 8) {
						address += 56;
						xxx = 0;
					  }
					  if(address > 0x17fff)
						address -= 0x8000;
					}
				  }
				} else {
				  if(a1 & 0x2000)
					t = sizeY - t - 1;
				  int c = (a2 & 0x3FF);
				  if((DISPCNT & 7) > 2 && (c < 512))
					continue;

				  int inc = 32;
				  if(DISPCNT & 0x40) {
					inc = sizeX >> 3;
				  }
				  int xxx = 0;
				  if(a1 & 0x1000)
					xxx = sizeX - 1;

					if(a0 & 0x1000) {
					  t -= (t % mosaicY);
					}

				  int address = 0x10000 + ((((c + (t>>3) * inc)<<5)
					+ ((t & 7)<<2) + ((xxx>>3)<<5) + ((xxx & 7) >> 1))&0x7FFF);
				  u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);
				  int palette = (a2 >> 8) & 0xF0;
				  if(a1 & 0x1000) {
					xxx = 7;
					for(int xx = sizeX - 1; xx >= 0; xx--) {
					  if (xx >= startpix)
						lineOBJpix--;
					  if (lineOBJpix<0)
						continue;
					  if(sx < 240) {
						u8 color = vram[address];
						if(xx & 1) {
						  color = (color >> 4);
						} else
						  color &= 0x0F;

						if ((color==0) && (((prio >> 25)&3) <
										   ((lineOBJ[sx]>>25)&3))) {
						  lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
						  if((a0 & 0x1000) && m)
							lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
						} else if((color) && (prio < (lineOBJ[sx]&0xFF000000))) {
						  lineOBJ[sx] = READ16LE(&spritePalette[palette + color]) | prio;
						  if((a0 & 0x1000) && m)
							lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
						}
					  }
					  if (a0 & 0x1000) {
						m++;
						if (m==mosaicX)
						  m=0;
					  }
					  sx = (sx+1) & 511;
					  xxx--;
					  if(!(xx & 1))
						address--;
					  if(xxx == -1) {
						xxx = 7;
						address -= 28;
					  }
					  if(address < 0x10000)
						address += 0x8000;
					}
				  } else {
					for(int xx = 0; xx < sizeX; xx++) {
					  if (xx >= startpix)
						lineOBJpix--;
					  if (lineOBJpix<0)
						continue;
					  if(sx < 240) {
						u8 color = vram[address];
						if(xx & 1) {
						  color = (color >> 4);
						} else
						  color &= 0x0F;

						if ((color==0) && (((prio >> 25)&3) <
										   ((lineOBJ[sx]>>25)&3))) {
						  lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
						  if((a0 & 0x1000) && m)
							lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
						} else if((color) && (prio < (lineOBJ[sx]&0xFF000000))) {
						  lineOBJ[sx] = READ16LE(&spritePalette[palette + color]) | prio;
						  if((a0 & 0x1000) && m)
							lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;

						}
					  }
					  if (a0 & 0x1000) {
						m++;
						if (m==mosaicX)
						  m=0;
					  }
					  sx = (sx+1) & 511;
					  xxx++;
					  if(xx & 1)
						address++;
					  if(xxx == 8) {
						address += 28;
						xxx = 0;
					  }
					  if(address > 0x17fff)
						address -= 0x8000;
					}
				  }
				}
			  }
			}
		}
	}
}

void gfxDrawOBJWin(u32 *lineOBJWin)
{
  gfxClearArray(lineOBJWin);
  if((layerEnable & 0x9000) == 0x9000) {
    u16 *sprites = (u16 *)oam;
    // u16 *spritePalette = &((u16 *)paletteRAM)[256];
    for(int x = 0; x < 128 ; x++) {
      int lineOBJpix = lineOBJpixleft[x];
      u16 a0 = READ16LE(sprites++);
      u16 a1 = READ16LE(sprites++);
      u16 a2 = READ16LE(sprites++);
      sprites++;

      if (lineOBJpix<=0)
        continue;

      // ignores non OBJ-WIN and disabled OBJ-WIN
      if(((a0 & 0x0c00) != 0x0800) || ((a0 & 0x0300) == 0x0200))
        continue;

      if ((a0 & 0x0c00) == 0x0c00)
        a0 &=0xF3FF;

      if ((a0>>14) == 3)
      {
        a0 &= 0x3FFF;
        a1 &= 0x3FFF;
      }

      int sizeX = 8<<(a1>>14);
      int sizeY = sizeX;

      if ((a0>>14) & 1)
      {
        if (sizeX<32)
          sizeX<<=1;
        if (sizeY>8)
          sizeY>>=1;
      }
      else if ((a0>>14) & 2)
      {
        if (sizeX>8)
          sizeX>>=1;
        if (sizeY<32)
          sizeY<<=1;
      }

      int sy = (a0 & 255);

      if(a0 & 0x0100) {
        int fieldX = sizeX;
        int fieldY = sizeY;
        if(a0 & 0x0200) {
          fieldX <<= 1;
          fieldY <<= 1;
        }
        if((sy+fieldY) > 256)
          sy -= 256;
        int t = VCOUNT - sy;
        if((t >= 0) && (t < fieldY)) {
          int sx = (a1 & 0x1FF);
          int startpix = 0;
          if ((sx+fieldX)> 512)
          {
            startpix=512-sx;
          }
          if((sx < 240) || startpix) {
            lineOBJpix-=8;
            // int t2 = t - (fieldY >> 1);
            int rot = (a1 >> 9) & 0x1F;
            u16 *OAM = (u16 *)oam;
            int dx = READ16LE(&OAM[3 + (rot << 4)]);
            if(dx & 0x8000)
              dx |= 0xFFFF8000;
            int dmx = READ16LE(&OAM[7 + (rot << 4)]);
            if(dmx & 0x8000)
              dmx |= 0xFFFF8000;
            int dy = READ16LE(&OAM[11 + (rot << 4)]);
            if(dy & 0x8000)
              dy |= 0xFFFF8000;
            int dmy = READ16LE(&OAM[15 + (rot << 4)]);
            if(dmy & 0x8000)
              dmy |= 0xFFFF8000;

            int realX = ((sizeX) << 7) - (fieldX >> 1)*dx - (fieldY>>1)*dmx
              + t * dmx;
            int realY = ((sizeY) << 7) - (fieldX >> 1)*dy - (fieldY>>1)*dmy
              + t * dmy;

            // u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);

            if(a0 & 0x2000) {
              int c = (a2 & 0x3FF);
              if((DISPCNT & 7) > 2 && (c < 512))
                continue;
              int inc = 32;
              if(DISPCNT & 0x40)
                inc = sizeX >> 2;
              else
                c &= 0x3FE;
              for(int x = 0; x < fieldX; x++) {
                if (x >= startpix)
                  lineOBJpix-=2;
                if (lineOBJpix<0)
                  continue;
                int xxx = realX >> 8;
                int yyy = realY >> 8;

                if(xxx < 0 || xxx >= sizeX ||
                   yyy < 0 || yyy >= sizeY ||
                   sx >= 240) {
                } else {
                  u32 color = vram[0x10000 + ((((c + (yyy>>3) * inc)<<5)
                                    + ((yyy & 7)<<3) + ((xxx >> 3)<<6) +
                                   (xxx & 7))&0x7fff)];
                  if(color) {
                    lineOBJWin[sx] = 1;
                  }
                }
                sx = (sx+1)&511;
                realX += dx;
                realY += dy;
              }
            } else {
              int c = (a2 & 0x3FF);
              if((DISPCNT & 7) > 2 && (c < 512))
                continue;

              int inc = 32;
              if(DISPCNT & 0x40)
                inc = sizeX >> 3;
              // int palette = (a2 >> 8) & 0xF0;
              for(int x = 0; x < fieldX; x++) {
                if (x >= startpix)
                  lineOBJpix-=2;
                if (lineOBJpix<0)
                  continue;
                int xxx = realX >> 8;
                int yyy = realY >> 8;

                //              if(x == 0 || x == (sizeX-1) ||
                //                 t == 0 || t == (sizeY-1)) {
                //                lineOBJ[sx] = 0x001F | prio;
                //              } else {
                  if(xxx < 0 || xxx >= sizeX ||
                     yyy < 0 || yyy >= sizeY ||
                     sx >= 240) {
                  } else {
                    u32 color = vram[0x10000 + ((((c + (yyy>>3) * inc)<<5)
                                     + ((yyy & 7)<<2) + ((xxx >> 3)<<5) +
                                     ((xxx & 7)>>1))&0x7fff)];
                    if(xxx & 1)
                      color >>= 4;
                    else
                      color &= 0x0F;

                    if(color) {
                      lineOBJWin[sx] = 1;
                    }
                  }
                  //            }
                sx = (sx+1)&511;
                realX += dx;
                realY += dy;
              }
            }
          }
        }
      } else {
        if((sy+sizeY) > 256)
          sy -= 256;
        int t = VCOUNT - sy;
        if((t >= 0) && (t < sizeY)) {
          int sx = (a1 & 0x1FF);
          int startpix = 0;
          if ((sx+sizeX)> 512)
          {
            startpix=512-sx;
          }
          if((sx < 240) || startpix) {
            lineOBJpix+=2;
            if(a0 & 0x2000) {
              if(a1 & 0x2000)
                t = sizeY - t - 1;
              int c = (a2 & 0x3FF);
              if((DISPCNT & 7) > 2 && (c < 512))
                continue;

              int inc = 32;
              if(DISPCNT & 0x40) {
                inc = sizeX >> 2;
              } else {
                c &= 0x3FE;
              }
              int xxx = 0;
              if(a1 & 0x1000)
                xxx = sizeX-1;
              int address = 0x10000 + ((((c+ (t>>3) * inc) << 5)
                + ((t & 7) << 3) + ((xxx>>3)<<6) + (xxx & 7))&0x7fff);
              if(a1 & 0x1000)
                xxx = 7;
              // u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);
              for(int xx = 0; xx < sizeX; xx++) {
                if (xx >= startpix)
                  lineOBJpix--;
                if (lineOBJpix<0)
                  continue;
                if(sx < 240) {
                  u8 color = vram[address];
                  if(color) {
                    lineOBJWin[sx] = 1;
                  }
                }

                sx = (sx+1) & 511;
                if(a1 & 0x1000) {
                  xxx--;
                  address--;
                  if(xxx == -1) {
                    address -= 56;
                    xxx = 7;
                  }
                  if(address < 0x10000)
                    address += 0x8000;
                } else {
                  xxx++;
                  address++;
                  if(xxx == 8) {
                    address += 56;
                    xxx = 0;
                  }
                  if(address > 0x17fff)
                    address -= 0x8000;
                }
              }
            } else {
              if(a1 & 0x2000)
                t = sizeY - t - 1;
              int c = (a2 & 0x3FF);
              if((DISPCNT & 7) > 2 && (c < 512))
                continue;

              int inc = 32;
              if(DISPCNT & 0x40) {
                inc = sizeX >> 3;
              }
              int xxx = 0;
              if(a1 & 0x1000)
                xxx = sizeX - 1;
              int address = 0x10000 + ((((c + (t>>3) * inc)<<5)
                + ((t & 7)<<2) + ((xxx>>3)<<5) + ((xxx & 7) >> 1))&0x7fff);
              // u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);
              // int palette = (a2 >> 8) & 0xF0;
              if(a1 & 0x1000) {
                xxx = 7;
                for(int xx = sizeX - 1; xx >= 0; xx--) {
                  if (xx >= startpix)
                    lineOBJpix--;
                  if (lineOBJpix<0)
                    continue;
                  if(sx < 240) {
                    u8 color = vram[address];
                    if(xx & 1) {
                      color = (color >> 4);
                    } else
                      color &= 0x0F;

                    if(color) {
                      lineOBJWin[sx] = 1;
                    }
                  }
                  sx = (sx+1) & 511;
                  xxx--;
                  if(!(xx & 1))
                    address--;
                  if(xxx == -1) {
                    xxx = 7;
                    address -= 28;
                  }
                  if(address < 0x10000)
                    address += 0x8000;
                }
              } else {
                for(int xx = 0; xx < sizeX; xx++) {
                  if (xx >= startpix)
                    lineOBJpix--;
                  if (lineOBJpix<0)
                    continue;
                  if(sx < 240) {
                    u8 color = vram[address];
                    if(xx & 1) {
                      color = (color >> 4);
                    } else
                      color &= 0x0F;

                    if(color) {
                      lineOBJWin[sx] = 1;
                    }
                  }
                  sx = (sx+1) & 511;
                  xxx++;
                  if(xx & 1)
                    address++;
                  if(xxx == 8) {
                    address += 28;
                    xxx = 0;
                  }
                  if(address > 0x17fff)
                    address -= 0x8000;
                }
              }
            }
          }
        }
      }
    }
  }
}

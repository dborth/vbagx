/*
Mode 1 is a tiled graphics mode, but with background layer 2 supporting scaling and rotation.
There is no layer 3 in this mode.
Layers 0 and 1 can be either 16 colours (with 16 different palettes) or 256 colours. 
There are 1024 tiles available.
Layer 2 is 256 colours and allows only 256 tiles.

These routines only render a single line at a time, because of the way the GBA does events.
*/
#include "GBA.h"
#include "Globals.h"
#include "GBAGfx.h"

void mode1RenderLine()
{
  u16 *palette = (u16 *)paletteRAM;

  if(DISPCNT & 0x80) {

	int x = 232;	//240 - 8  
	do{
		lineMix[x  ] =
		lineMix[x+1] =
		lineMix[x+2] =
		lineMix[x+3] =
		lineMix[x+4] =
		lineMix[x+5] =
		lineMix[x+6] =
		lineMix[x+7] = 0x7fff;
		x-=8;
	}while(x>=0);

    gfxLastVCOUNT = VCOUNT;
    return;
  }

  if(layerEnable & 0x0100) {
    gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
  }

  if(layerEnable & 0x0200) {
    gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
  }

  if(layerEnable & 0x0400) {
    int changed = gfxBG2Changed;
    if(gfxLastVCOUNT > VCOUNT)
      changed = 3;
    gfxDrawRotScreen(BG2CNT, BG2X_L, BG2X_H, BG2Y_L, BG2Y_H,
                     BG2PA, BG2PB, BG2PC, BG2PD,
                     gfxBG2X, gfxBG2Y, changed, line2);
  }

  gfxDrawSprites(lineOBJ);

  u32 backdrop;
  if(customBackdropColor == -1) {
    backdrop = (READ16LE(&palette[0]) | 0x30000000);
  } else {
    backdrop = ((customBackdropColor & 0x7FFF) | 0x30000000);
  }

  for(u32 x = 0; x < 240u; ++x) {
    u32 color = backdrop;
    u8 top = 0x20;

	u8 li1 = (u8)(line1[x]>>24);
	u8 li2 = (u8)(line2[x]>>24);
	u8 li4 = (u8)(lineOBJ[x]>>24);	
	
	u8 r = 	(li2 < li1) ? (li2) : (li1);
	
	if(li4 < r){
		r = 	(li4);
	}
	
	if(line0[x] < backdrop) {
	  color = line0[x];
	  top = 0x01;
	}
	
	if(r < (u8)(color >> 24)) {
		if(r == li1){
			color = line1[x];
			top = 0x02;
		}else if(r == li2){
			color = line2[x];
			top = 0x04;
		}else if(r == li4){
			color = lineOBJ[x];
			top = 0x10;
		}
	}

    if((top & 0x10) && (color & 0x00010000)) {
      // semi-transparent OBJ
      u32 back = backdrop;
      u8 top2 = 0x20;

		u8 li0 = (u8)(line0[x]>>24);
		u8 li1 = (u8)(line1[x]>>24);
		u8 li2 = (u8)(line2[x]>>24);
		u8 r = 	(li1 < li0) ? (li1) : (li0);
		
		if(li2 < r) {
			r =  (li2);
		}
		
		if(r < (u8)(back >> 24)) {
			if(r == li0){
				back = line0[x];
				top2 = 0x01;
			}else if(r == li1){
				back = line1[x];
				top2 = 0x02;
			}else if(r == li2){
				back = line2[x];
				top2 = 0x04;
			}
		}

      if(top2 & (BLDMOD>>8))
        color = gfxAlphaBlend(color, back,
                              coeff[COLEV & 0x1F],
                              coeff[(COLEV >> 8) & 0x1F]);
      else {
        switch((BLDMOD >> 6) & 3) {
        case 2:
          if(BLDMOD & top)
            color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        case 3:
          if(BLDMOD & top)
            color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        }
      }
    }

    lineMix[x] = color;
  }
  gfxBG2Changed = 0;
  gfxLastVCOUNT = VCOUNT;
}

void mode1RenderLineNoWindow()
{
  u16 *palette = (u16 *)paletteRAM;

  if(DISPCNT & 0x80) {

	int x = 232;	//240 -  8  
	do{
		lineMix[x  ] =
		lineMix[x+1] =
		lineMix[x+2] =
		lineMix[x+3] =
		lineMix[x+4] =
		lineMix[x+5] =
		lineMix[x+6] =
		lineMix[x+7] = 0x7fff;
		x-=8;
	}while(x>=0);

    gfxLastVCOUNT = VCOUNT;
    return;
  }

  if(layerEnable & 0x0100) {
    gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
  }


  if(layerEnable & 0x0200) {
    gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
  }

  if(layerEnable & 0x0400) {
    int changed = gfxBG2Changed;
    if(gfxLastVCOUNT > VCOUNT)
      changed = 3;
    gfxDrawRotScreen(BG2CNT, BG2X_L, BG2X_H, BG2Y_L, BG2Y_H,
                     BG2PA, BG2PB, BG2PC, BG2PD,
                     gfxBG2X, gfxBG2Y, changed, line2);
  }

  gfxDrawSprites(lineOBJ);

  u32 backdrop;
  if(customBackdropColor == -1) {
    backdrop = (READ16LE(&palette[0]) | 0x30000000);
  } else {
    backdrop = ((customBackdropColor & 0x7FFF) | 0x30000000);
  }

  for(int x = 0; x < 240; ++x) {
    u32 color = backdrop;
    u8 top = 0x20;

	u8 li1 = (u8)(line1[x]>>24);
	u8 li2 = (u8)(line2[x]>>24);
	u8 li4 = (u8)(lineOBJ[x]>>24);	
	
	u8 r = 	(li2 < li1) ? (li2) : (li1);
	
	if(li4 < r){
		r = 	(li4);
	}
	
	if(line0[x] < backdrop) {
	  color = line0[x];
	  top = 0x01;
	}
	
	if(r < (u8)(color >> 24)) {
		if(r == li1){
			color = line1[x];
			top = 0x02;
		}else if(r == li2){
			color = line2[x];
			top = 0x04;
		}else if(r == li4){
			color = lineOBJ[x];
			top = 0x10;
		}
	}

    if(!(color & 0x00010000)) {
      switch((BLDMOD >> 6) & 3) {
      case 0:
        break;
      case 1:
        {
          if(top & BLDMOD) {
            u32 back = backdrop;
            u8 top2 = 0x20;

			if((top != 0x01) && (u8)(line0[x]>>24) < (u8)(back >> 24)) {
				back = line0[x];
				top2 = 0x01;
			}

			if((top != 0x02) && (u8)(line1[x]>>24) < (u8)(back >> 24)) {
				back = line1[x];
				top2 = 0x02;
			}

			if((top != 0x04) && (u8)(line2[x]>>24) < (u8)(back >> 24)) {
				back = line2[x];
				top2 = 0x04;
			}

			if((top != 0x10) && (u8)(lineOBJ[x]>>24) < (u8)(back >> 24)) {
				back = lineOBJ[x];
				top2 = 0x10;
			}

            if(top2 & (BLDMOD>>8))
              color = gfxAlphaBlend(color, back,
                                    coeff[COLEV & 0x1F],
                                    coeff[(COLEV >> 8) & 0x1F]);
          }
        }
        break;
      case 2:
        if(BLDMOD & top)
          color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
        break;
      case 3:
        if(BLDMOD & top)
          color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
        break;
      }
    } else {
        // semi-transparent OBJ
        u32 back = backdrop;
        u8 top2 = 0x20;

		u8 li0 = (u8)(line0[x]>>24);
		u8 li1 = (u8)(line1[x]>>24);
		u8 li2 = (u8)(line2[x]>>24);	
		
		u8 r = 	(li1 < li0) ? (li1) : (li0);
		
		if(li2 < r) {
			r =  (li2);
		}
		
		if(r < (u8)(back >> 24)) {
			if(r == li0){
				back = line0[x];
				top2 = 0x01;
			}else if(r == li1){
				back = line1[x];
				top2 = 0x02;
			}else if(r == li2){
				back = line2[x];
				top2 = 0x04;
			}
		}

      if(top2 & (BLDMOD>>8))
        color = gfxAlphaBlend(color, back,
                              coeff[COLEV & 0x1F],
                              coeff[(COLEV >> 8) & 0x1F]);
      else {
        switch((BLDMOD >> 6) & 3) {
        case 2:
          if(BLDMOD & top)
            color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        case 3:
          if(BLDMOD & top)
            color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        }
      }
    }

    lineMix[x] = color;
  }
  gfxBG2Changed = 0;
  gfxLastVCOUNT = VCOUNT;
}

void mode1RenderLineAll()
{
  u16 *palette = (u16 *)paletteRAM;

  if(DISPCNT & 0x80) {

	int x = 232;	//240 -  8  
	do{
		lineMix[x  ] =
		lineMix[x+1] =
		lineMix[x+2] =
		lineMix[x+3] =
		lineMix[x+4] =
		lineMix[x+5] =
		lineMix[x+6] =
		lineMix[x+7] = 0x7fff;
		x-=8;
	}while(x>=0);

    gfxLastVCOUNT = VCOUNT;
    return;
  }

  bool inWindow0 = false;
  bool inWindow1 = false;

  if(layerEnable & 0x2000) {
    u8 v0 = WIN0V >> 8;
    u8 v1 = WIN0V & 255;
    inWindow0 = ((v0 == v1) && (v0 >= 0xe8));
    if(v1 >= v0)
      inWindow0 |= (VCOUNT >= v0 && VCOUNT < v1);
    else
      inWindow0 |= (VCOUNT >= v0 || VCOUNT < v1);
  }
  if(layerEnable & 0x4000) {
    u8 v0 = WIN1V >> 8;
    u8 v1 = WIN1V & 255;
    inWindow1 = ((v0 == v1) && (v0 >= 0xe8));
    if(v1 >= v0)
      inWindow1 |= (VCOUNT >= v0 && VCOUNT < v1);
    else
      inWindow1 |= (VCOUNT >= v0 || VCOUNT < v1);
  }

  if(layerEnable & 0x0100) {
    gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
  }

  if(layerEnable & 0x0200) {
    gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
  }

  if(layerEnable & 0x0400) {
    int changed = gfxBG2Changed;
    if(gfxLastVCOUNT > VCOUNT)
      changed = 3;
    gfxDrawRotScreen(BG2CNT, BG2X_L, BG2X_H, BG2Y_L, BG2Y_H,
                     BG2PA, BG2PB, BG2PC, BG2PD,
                     gfxBG2X, gfxBG2Y, changed, line2);
  }

  gfxDrawSprites(lineOBJ);
  gfxDrawOBJWin(lineOBJWin);

  u32 backdrop;
  if(customBackdropColor == -1) {
    backdrop = (READ16LE(&palette[0]) | 0x30000000);
  } else {
    backdrop = ((customBackdropColor & 0x7FFF) | 0x30000000);
  }

  u8 inWin0Mask = WININ & 0xFF;
  u8 inWin1Mask = WININ >> 8;
  u8 outMask = WINOUT & 0xFF;

  for(int x = 0; x < 240; ++x) {
    u32 color = backdrop;
    u8 top = 0x20;
    u8 mask = outMask;

    if(!(lineOBJWin[x] & 0x80000000)) {
      mask = WINOUT >> 8;
    }

    if(inWindow1) {
      if(gfxInWin1[x])
        mask = inWin1Mask;
    }

    if(inWindow0) {
      if(gfxInWin0[x]) {
        mask = inWin0Mask;
      }
    }

	// At the very least, move the inexpensive 'mask' operation up front
    if((mask & 1) && line0[x] < backdrop) {
      color = line0[x];
      top = 0x01;
    }

    if((mask & 2) && (u8)(line1[x]>>24) < (u8)(color >> 24)) {
      color = line1[x];
      top = 0x02;
    }

    if((mask & 4) && (u8)(line2[x]>>24) < (u8)(color >> 24)) {
      color = line2[x];
      top = 0x04;
    }

    if((mask & 16) && (u8)(lineOBJ[x]>>24) < (u8)(color >> 24)) {
      color = lineOBJ[x];
      top = 0x10;
    }

    if(color & 0x00010000) {
      // semi-transparent OBJ
      u32 back = backdrop;
      u8 top2 = 0x20;

      if((mask & 1) && (u8)(line0[x]>>24) < (u8)(backdrop >> 24)) {
        back = line0[x];
        top2 = 0x01;
      }

      if((mask & 2) && (u8)(line1[x]>>24) < (u8)(back >> 24)) {
        back = line1[x];
        top2 = 0x02;
      }

      if((mask & 4) && (u8)(line2[x]>>24) < (u8)(back >> 24)) {
        back = line2[x];
        top2 = 0x04;
      }

      if(top2 & (BLDMOD>>8))
        color = gfxAlphaBlend(color, back,
                              coeff[COLEV & 0x1F],
                              coeff[(COLEV >> 8) & 0x1F]);
      else {
        switch((BLDMOD >> 6) & 3) {
        case 2:
          if(BLDMOD & top)
            color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        case 3:
          if(BLDMOD & top)
            color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        }
      }
    } else if(mask & 32) {
      // special FX on the window
      switch((BLDMOD >> 6) & 3) {
      case 0:
        break;
      case 1:
        {
          if(top & BLDMOD) {
            u32 back = backdrop;
            u8 top2 = 0x20;

            if((mask & 1) && (top != 0x01) && (u8)(line0[x]>>24) < (u8)(backdrop >> 24)) {
                back = line0[x];
                top2 = 0x01;
            }

            if((mask & 2) && (top != 0x02) && (u8)(line1[x]>>24) < (u8)(back >> 24)) {
                back = line1[x];
                top2 = 0x02;
            }

            if((mask & 4) && (top != 0x04) && (u8)(line2[x]>>24) < (u8)(back >> 24)) {
                back = line2[x];
                top2 = 0x04;
            }

            if((mask & 16) && (top != 0x10) && (u8)(lineOBJ[x]>>24) < (u8)(back >> 24)) {
                back = lineOBJ[x];
                top2 = 0x10;
            }

            if(top2 & (BLDMOD>>8))
              color = gfxAlphaBlend(color, back,
                                    coeff[COLEV & 0x1F],
                                    coeff[(COLEV >> 8) & 0x1F]);
          }
        }
        break;
      case 2:
        if(BLDMOD & top)
          color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
        break;
      case 3:
        if(BLDMOD & top)
          color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
        break;
      }
    }

    lineMix[x] = color;
  }
  gfxBG2Changed = 0;
  gfxLastVCOUNT = VCOUNT;
}

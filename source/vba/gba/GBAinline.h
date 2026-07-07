#ifndef GBAINLINE_H
#define GBAINLINE_H

#include "../System.h"
#include "../common/Port.h"
#include "RTC.h"
#include "Sound.h"
#include "vmmem.h" // Nintendo GC Virtual Memory

extern const u32 objTilesAddress[3];

extern bool stopState;
extern bool holdState;
extern int holdType;
extern int cpuNextEvent;
extern bool cpuSramEnabled;
extern bool cpuFlashEnabled;
extern bool cpuEEPROMEnabled;
extern bool cpuEEPROMSensorEnabled;
extern bool cpuDmaHack;
extern u32 cpuDmaLast;
extern bool timer0On;
extern int timer0Ticks;
extern int timer0ClockReload;
extern bool timer1On;
extern int timer1Ticks;
extern int timer1ClockReload;
extern bool timer2On;
extern int timer2Ticks;
extern int timer2ClockReload;
extern bool timer3On;
extern int timer3Ticks;
extern int timer3ClockReload;
extern int cpuTotalTicks;
extern u32 RomIdCode;

# define LIKELY(x) __builtin_expect(!!(x),1)
# define UNLIKELY(x) __builtin_expect(!!(x),0)

#define gid(a,b,c) (a|(b<<8)|(c<<16))
#define CORVETTE		gid('A','V','C')

/*****************************************************************************
 * Nintendo GameCube Virtual Memory function override (not for Wii)
 * Tantric September 2008
 ****************************************************************************/

#define CPUReadByteQuickDef(addr) \
  map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]

#define CPUReadHalfWordQuickDef(addr) \
  READ16LE(((u16*)&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]))

#define CPUReadMemoryQuickDef(addr) \
  READ32LE(((u32*)&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]))

u8 inline CPUReadByteQuick( u32 addr )
{
	switch(addr >> 24 )
	{
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0C:
#ifdef USE_VM
			return VMRead8( addr & 0x1FFFFFF );
#endif
		default:
			return CPUReadByteQuickDef(addr);
	}

	return 0;
}

u16 inline CPUReadHalfWordQuick( u32 addr )
{
	switch(addr >> 24)
	{
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0C:
#ifdef USE_VM
			return VMRead16( addr & 0x1FFFFFF );
#endif
		default:
			return CPUReadHalfWordQuickDef(addr);
	}

	return 0;
}

u32 inline CPUReadMemoryQuick( u32 addr )
{
	switch(addr >> 24)
	{
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0C:
#ifdef USE_VM
			return VMRead32( addr & 0x1FFFFFF );
#endif
		default:
			return CPUReadMemoryQuickDef(addr);
	}

	return 0;
}

/*****************************************************************************
 * End of VM override
 ****************************************************************************/

#ifdef USE_VM
#define SAFE_QUICK_READ32(addr) CPUReadMemoryQuick(addr)
#define SAFE_QUICK_READ16(addr) CPUReadHalfWordQuick(addr)
#define SAFE_QUICK_READ8(addr) CPUReadByteQuick(addr)
#else
#define SAFE_QUICK_READ32(addr) CPUReadMemoryQuickDef(addr)
#define SAFE_QUICK_READ16(addr) CPUReadHalfWordQuickDef(addr)
#define SAFE_QUICK_READ8(addr) CPUReadByteQuickDef(addr)
#endif

#define FALLBACK_UNREADABLE_32() \
    do { \
        if(cpuDmaHack) { value = cpuDmaLast; } \
        else if(armState) { value = SAFE_QUICK_READ32(reg[15].I); } \
        else { value = SAFE_QUICK_READ16(reg[15].I) | (SAFE_QUICK_READ16(reg[15].I) << 16); } \
    } while(0)

#define FALLBACK_UNREADABLE_16() \
    do { \
        if(cpuDmaHack) { value = cpuDmaLast & 0xFFFF; } \
        else if(armState) { value = SAFE_QUICK_READ16(reg[15].I + (address & 2)); } \
        else { value = SAFE_QUICK_READ16(reg[15].I); } \
    } while(0)

#define FALLBACK_UNREADABLE_8() \
    do { \
        if(cpuDmaHack) { return cpuDmaLast & 0xFF; } \
        else if(armState) { return SAFE_QUICK_READ8(reg[15].I + (address & 3)); } \
        else { return SAFE_QUICK_READ8(reg[15].I + (address & 1)); } \
    } while(0)

static inline u32 CPUReadMemory(u32 address)
{
  // OPTIMIZATION: Broadway evaluates this unconditional bitwise mask in 1 cycle (rlwinm).
  // This avoids a costly 2-3 cycle conditional branch if it were an if-statement.
  u32 alignedAddress = address & ~0x03;
  u32 value = 0;
  u8 pageIdx = alignedAddress >> 24;
  u8* base = gbaMemoryPages[pageIdx].base;

  // FAST PATH: Branchless lookup mapped perfectly to Broadway architecture
  if (LIKELY(base != NULL)) {
      value = READ32LE((u32*)(base + (alignedAddress & gbaMemoryPages[pageIdx].mask)));
  }
  // SLOW PATH: Legacy fallbacks for MMIO, Bios protection, EEPROM, Sensors, and Virtual Memory
  else {
      switch(pageIdx) {
      case 0:
          if(reg[15].I >> 24) {
            if(alignedAddress < 0x4000) value = READ32LE(((u32 *)&biosProtected));
            else { FALLBACK_UNREADABLE_32(); break; }
          } else {
            value = READ32LE(((u32 *)&bios[alignedAddress & 0x3FFC]));
          }
        break;
      case 4:
          // OPTIMIZATION: Converted short-circuit && to bitwise & to prevent costly pipeline flushes.
          if(LIKELY(((alignedAddress & 0x00FFFFFF) < 0x400) & ioReadable[alignedAddress & 0x3fc])) {
            if(ioReadable[(alignedAddress & 0x3fc) + 2]) value = READ32LE(((u32 *)&ioMem[alignedAddress & 0x3fC]));
            else value = READ16LE(((u16 *)&ioMem[alignedAddress & 0x3fc]));
          } else { FALLBACK_UNREADABLE_32(); break; }
        break;
      case 8:
      case 9:
      case 10:
      case 11:
      case 12:
#ifdef USE_VM // Nintendo GC Virtual Memory
          value = VMRead32( alignedAddress & 0x1FFFFFC );
#endif
        break;
      case 13:
        value = eepromRead(alignedAddress);
        break;
      case 14:
      case 15:
    // Yoshi's Universal Gravitation (Topsy Turvy)
	// Koro Koro
        if(UNLIKELY(cpuEEPROMSensorEnabled)) {
          switch(alignedAddress & 0x00008f00) {
          case 0x8200: value = systemGetSensorX() & 255; break;
          case 0x8300: value = (systemGetSensorX() >> 8)|0x80; break;
          case 0x8400: value = systemGetSensorY() & 255; break;
          case 0x8500: value = systemGetSensorY() >> 8; break;
          default: {
        // OPTIMIZATION: Replaced multi-cycle mullw with shifts and bitwise ORs.
            u32 temp = flashRead(alignedAddress);
            temp |= (temp << 8);
            value = temp | (temp << 16);
            break;
          }
          }
        } else {
      // OPTIMIZATION: Replaced multi-cycle mullw with shifts and bitwise ORs.
          u32 temp = flashRead(alignedAddress);
          temp |= (temp << 8);
          value = temp | (temp << 16);
        }
        break;
      default:
        FALLBACK_UNREADABLE_32();
        break;
      }
  }

  // OPTIMIZATION: Predict unaligned accesses as UNLIKELY.
  // The shift is mapped branchlessly but the check prevents UB in C.
  if(UNLIKELY(address & 3)) {
    u32 shift = (address & 3) << 3;
    value = (value >> shift) | (value << (32 - shift));
  }

  return value;
}


static inline u32 CPUReadHalfWord(u32 address)
{
  // OPTIMIZATION: Unconditional rlwinm mask
  u32 alignedAddress = address & ~0x01;
  u32 value = 0;
  u8 pageIdx = alignedAddress >> 24;
  u8* base = gbaMemoryPages[pageIdx].base;

  // FAST PATH
  if (LIKELY(base != NULL)) {
      value = READ16LE((u16*)(base + (alignedAddress & gbaMemoryPages[pageIdx].mask)));
  }
  // SLOW PATH
  else {
      switch(pageIdx) {
      case 0:
        if (reg[15].I >> 24) {
          if(alignedAddress < 0x4000) value = READ16LE(((u16 *)&biosProtected[alignedAddress & 2]));
          else { FALLBACK_UNREADABLE_16(); }
        } else {
          value = READ16LE(((u16 *)&bios[alignedAddress & 0x3FFE]));
        }
        break;
      case 4:
        if(LIKELY((alignedAddress & 0x00FFFFFF) < 0x400)) {
          u32 ioAddr = alignedAddress & 0x3fe;

          if(ioReadable[ioAddr]) {
            value = READ16LE(((u16 *)&ioMem[ioAddr]));
            if (UNLIKELY((u32)(ioAddr - 0x100) < 0x10)) {
              if ((ioAddr == 0x100) & timer0On)
                value = 0xFFFF - ((timer0Ticks - cpuTotalTicks) >> timer0ClockReload);
              else if ((ioAddr == 0x104) & timer1On & !(TM1CNT & 4))
                value = 0xFFFF - ((timer1Ticks - cpuTotalTicks) >> timer1ClockReload);
              else if ((ioAddr == 0x108) & timer2On & !(TM2CNT & 4))
                value = 0xFFFF - ((timer2Ticks - cpuTotalTicks) >> timer2ClockReload);
              else if ((ioAddr == 0x10C) & timer3On & !(TM3CNT & 4))
                value = 0xFFFF - ((timer3Ticks - cpuTotalTicks) >> timer3ClockReload);
            }
          } else if(ioReadable[alignedAddress & 0x3fc]) {
            value = 0;
          } else { FALLBACK_UNREADABLE_16(); }
        } else { FALLBACK_UNREADABLE_16(); }
        break;
      case 8:
        // This is possibly the GPIO port that controls the real time clock,
		// WarioWare Twisted! tilt sensors, rumble, and solar sensors.
		// This function still works if there is no real time clock
		// and does a normal memory read in that case.
		// OPTIMIZATION: Compacted dual-boundary check down to a highly optimized single unsigned compare.
        if(UNLIKELY((u32)(alignedAddress - 0x80000c4) <= 4)) {
          value = rtcRead(alignedAddress & 0xFFFFFFE);
          break;
        }
      case 9:
      case 10:
      case 11:
      case 12:
#ifdef USE_VM
        value = VMRead16( alignedAddress & 0x1FFFFFE );
#endif
        break;
      case 13:
        value = eepromRead(alignedAddress);
        break;
      case 14:
      case 15:
        if(UNLIKELY(cpuEEPROMSensorEnabled)) {
          switch(alignedAddress & 0x00008f00) {
          case 0x8200: value = systemGetSensorX() & 255; break;
          case 0x8300: value = (systemGetSensorX() >> 8)|0x80; break;
          case 0x8400: value = systemGetSensorY() & 255; break;
          case 0x8500: value = systemGetSensorY() >> 8; break;
          default: {
            u32 temp = flashRead(alignedAddress);
            value = temp | (temp << 8);
            break;
          }
          }
        } else {
          u32 temp = flashRead(alignedAddress);
          value = temp | (temp << 8);
        }
        break;
      default:
        FALLBACK_UNREADABLE_16();
        break;
      }
  }

  // OPTIMIZATION: ROTR 8 branchless equivalent.
  if(UNLIKELY(address & 1)) {
    value = (value >> 8) | (value << 24);
  }

  return value;
}

static inline s16 CPUReadHalfWordSigned(u32 address)
{
  return (s16)CPUReadHalfWord(address);
}

static inline u8 CPUReadByte(u32 address)
{
  u8 pageIdx = address >> 24;
  u8* base = gbaMemoryPages[pageIdx].base;

  // FAST PATH
  if (LIKELY(base != NULL)) {
      return base[address & gbaMemoryPages[pageIdx].mask];
  }

  // SLOW PATH
  switch(pageIdx) {
  case 0:
    if (reg[15].I >> 24) {
      if(address < 0x4000) return biosProtected[address & 3];
      else { FALLBACK_UNREADABLE_8(); }
    }
    return bios[address & 0x3FFF];
  case 4:
     // OPTIMIZATION: Removed short circuit evaluation to prevent pipeline bubble stalls.
    if(LIKELY(((address & 0x00FFFFFF) < 0x400) & ioReadable[address & 0x3ff]))
      return ioMem[address & 0x3ff];
    else { FALLBACK_UNREADABLE_8(); }
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
#ifdef USE_VM 
    return VMRead8( address & 0x1FFFFFF );
#endif
    break; // Break here if undefined VM handler defaults.
  case 13:
    return eepromRead(address);
  case 14:
  case 15:
    // Yoshi's Universal Gravitation (Topsy Turvy)
	// Koro Koro
    if(UNLIKELY(cpuEEPROMSensorEnabled)) {
      switch(address & 0x00008f00) {
      case 0x8200: return systemGetSensorX() & 255;
      case 0x8300: return (systemGetSensorX() >> 8)|0x80;
      case 0x8400: return systemGetSensorY() & 255;
      case 0x8500: return systemGetSensorY() >> 8;
      }
    }
    return flashRead(address);
  default:
    break;
  }
  
  FALLBACK_UNREADABLE_8();
}

static inline void CPUWriteMemory(u32 address, u32 value)
{
  // OPTIMIZATION: ~0x03 maps cleanly to a 1-cycle rlwinm mask
  address &= ~0x03;

  switch(address >> 24) {
  case 0x02:
    WRITE32LE(((u32 *)&workRAM[address & 0x3FFFC]), value);
    break;
  case 0x03:
    WRITE32LE(((u32 *)&internalRAM[address & 0x7ffC]), value);
    break;
  case 0x04:
    if(LIKELY((address & 0x00FFFFFF) < 0x400)) {
      u32 ioAddr = address & 0x3FC;
      CPUUpdateRegister(ioAddr, value & 0xFFFF);
      CPUUpdateRegister(ioAddr + 2, (value >> 16));
    } else goto unwritable;
    break;
  case 0x05:
    // OPTIMIZATION: Combined validation checks bitwise to maintain pipeline fluidity.
    if(LIKELY(((address & 0x00FFFFFF) < 0x400) | ((RomIdCode & 0xFFFFFF) != CORVETTE)))
      WRITE32LE(((u32 *)&paletteRAM[address & 0x3FC]), value);
    break;
  case 0x06:
    address &= 0x1fffc;
    if (UNLIKELY(((DISPCNT & 7) > 2) & ((address & 0x1C000) == 0x18000)))
      return;
    // OPTIMIZATION: Branchless bitwise mirror masking sequence.
    address &= ~(((address & 0x10000) >> 1) & address);
    WRITE32LE(((u32 *)&vram[address]), value);
    break;
  case 0x07:
    WRITE32LE(((u32 *)&oam[address & 0x3fc]), value);
    break;
  case 0x0D:
    if(LIKELY(cpuEEPROMEnabled)) {
      eepromWrite(address, value);
      break;
    }
    goto unwritable;
  case 0x0E:
  case 0x0F:
    if((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled) {
      (*cpuSaveGameFunc)(address, (u8)value);
      break;
    }
  default:
  unwritable:
    break;
  }
}


static inline void CPUWriteHalfWord(u32 address, u16 value)
{
  // OPTIMIZATION: 1-cycle rlwinm
  address &= ~0x01;

  switch(address >> 24) {
  case 2:
    WRITE16LE(((u16 *)&workRAM[address & 0x3FFFE]),value);
    break;
  case 3:
    WRITE16LE(((u16 *)&internalRAM[address & 0x7ffe]), value);
    break;
  case 4:
    if(LIKELY((address & 0x00FFFFFF) < 0x400))
      CPUUpdateRegister(address & 0x3fe, value);
    else goto unwritable;
    break;
  case 5:
    if(LIKELY(((address & 0x00FFFFFF) < 0x400) | ((RomIdCode & 0xFFFFFF) != CORVETTE)))
      WRITE16LE(((u16 *)&paletteRAM[address & 0x3fe]), value);
    break;
  case 6:
    address &= 0x1fffe;
    if (UNLIKELY(((DISPCNT & 7) > 2) & ((address & 0x1C000) == 0x18000)))
      return;
    // OPTIMIZATION: Branchless bitwise mirror masking sequence.
    address &= ~(((address & 0x10000) >> 1) & address);
    WRITE16LE(((u16 *)&vram[address]), value);
    break;
  case 7:
    WRITE16LE(((u16 *)&oam[address & 0x3fe]), value);
    break;
  case 8:
  case 9:
    // OPTIMIZATION: Compacted multiple equality branches down into a single range execution.
    if(UNLIKELY((u32)(address - 0x80000c4) <= 4)) {
      if(!rtcWrite(address, value))
        goto unwritable;
    }
    break;
  case 13:
    if(LIKELY(cpuEEPROMEnabled)) {
      eepromWrite(address, (u8)value);
      break;
    }
    goto unwritable;
  case 14:
  case 15:
    if((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled) {
      (*cpuSaveGameFunc)(address, (u8)value);
      break;
    }
    goto unwritable;
  default:
  unwritable:
    break;
  }
}

static inline void CPUWriteByte(u32 address, u8 b)
{
  switch(address >> 24) {
  case 2:
    workRAM[address & 0x3FFFF] = b;
    break;
  case 3:
    internalRAM[address & 0x7fff] = b;
    break;
  case 4:
    if(LIKELY((address & 0x00FFFFFF) < 0x400)) {
      u32 ioAddr = address & 0x3FF;

      // OPTIMIZATION: HALTCNT separated to fast-path default registers
      if (UNLIKELY(ioAddr == 0x301)) {
        if(b == 0x80) stopState = true;
        holdState = 1;
        holdType = -1;
        cpuNextEvent = cpuTotalTicks;
        break;
      }

      // OPTIMIZATION: Branchless Sound Event Validation (SWAR-lite).
      // Eliminates the massive 40-case jump table completely.
      // 0x333F333F matches valid bits 0x60-0x7F. 0xFFFF0033 matches 0x80-0x9F.
      u32 sndOffset = ioAddr - 0x60;
      if (UNLIKELY(sndOffset <= 0x3F)) {
        // Pure arithmetic mask selection. Avoids array caching & ternary branch stalls.
        u32 highMask = -(sndOffset >> 5); // 0x00000000 if < 32, 0xFFFFFFFF if >= 32
        u32 validMask = (0x333F333F & ~highMask) | (0xFFFF0033 & highMask);

        if ((validMask >> (sndOffset & 31)) & 1) {
          soundEvent(address & 0xFF, b);
          break;
        }
      }

      // OPTIMIZATION: Branchless IO register Byte-Write.
      // Resolves exact shift boundaries using single-cycle shifts and adds.
      u32 lowerBits = ioAddr & ~0x01;
      u32 shift = (ioAddr & 1) << 3;        // Shift is 0 or 8
      u16 ioVal = READ16LE(&ioMem[lowerBits]);
      u16 mask = 0xFF00U >> shift;          // Mask is 0xFF00 or 0x00FF

      CPUUpdateRegister(lowerBits, (ioVal & mask) | (b << shift));

    } else goto unwritable;
    break;
  case 5:
    *((u16 *)&paletteRAM[address & 0x3FE]) = (b << 8) | b;
    break;
  case 6:
    address &= 0x1fffe;
    if (UNLIKELY(((DISPCNT & 7) > 2) & ((address & 0x1C000) == 0x18000)))
      return;
    // OPTIMIZATION: Branchless bitwise mirror masking sequence.
    address &= ~(((address & 0x10000) >> 1) & address);

    // Shift is used exclusively instead of division (/4)
    if (address < objTilesAddress[((DISPCNT&7)+1)>>2])
    {
      *((u16 *)&vram[address]) = (b << 8) | b;
    }
    break;
  case 7:
    break;
  case 13:
    if(LIKELY(cpuEEPROMEnabled)) {
      eepromWrite(address, b);
      break;
    }
    goto unwritable;
  case 14:
  case 15:
     // Bitwise OR preserved to avoid branch penalties
    if ((saveType != 5) & ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled)) {
      (*cpuSaveGameFunc)(address, b);
      break;
    }
  default:
  unwritable:
    break;
  }
}

#endif // GBAINLINE_H
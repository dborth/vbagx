#ifndef GBAINLINE_H
#define GBAINLINE_H

#include "../System.h"
#include "../common/Port.h"
#include "RTC.h"
#include "Sound.h"
#include "agbprint.h"
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

static inline u32 CPUReadMemory(u32 address)
{
  // OPTIMIZATION: Broadway evaluates this unconditional bitwise mask in 1 cycle (rlwinm).
  // This avoids a costly 2-3 cycle conditional branch if it were an if-statement.
  u32 alignedAddress = address & ~0x03;
  u32 value = 0;

  switch(alignedAddress >> 24) {
  case 0:
    if(reg[15].I >> 24) {
      if(alignedAddress < 0x4000) {
        value = READ32LE(((u32 *)&biosProtected));
      } else goto unreadable;
    } else {
      value = READ32LE(((u32 *)&bios[alignedAddress & 0x3FFC]));
    }
    break;
  case 2:
    value = READ32LE(((u32 *)&workRAM[alignedAddress & 0x3FFFC]));
    break;
  case 3:
    value = READ32LE(((u32 *)&internalRAM[alignedAddress & 0x7ffC]));
    break;
  case 4:
    if(LIKELY((alignedAddress < 0x4000400) && ioReadable[alignedAddress & 0x3fc])) {
      if(ioReadable[(alignedAddress & 0x3fc) + 2]) {
        value = READ32LE(((u32 *)&ioMem[alignedAddress & 0x3fC]));
      } else {
        value = READ16LE(((u16 *)&ioMem[alignedAddress & 0x3fc]));
      }
    } else {
      goto unreadable;
    }
    break;
  case 5:
    value = READ32LE(((u32 *)&paletteRAM[alignedAddress & 0x3fC]));
    break;
  case 6:
    alignedAddress = (alignedAddress & 0x1fffc);
    if (UNLIKELY(((DISPCNT & 7) > 2) && ((alignedAddress & 0x1C000) == 0x18000))) {
        value = 0;
        break;
    }
    if ((alignedAddress & 0x18000) == 0x18000)
      alignedAddress &= 0x17fff;
    value = READ32LE(((u32 *)&vram[alignedAddress]));
    break;
  case 7:
    value = READ32LE(((u32 *)&oam[alignedAddress & 0x3FC]));
    break;
  case 8:
    // Must be cartridge ROM, reading other sensors doesn't allow 32-bit access.
  case 9:
  case 10:
  case 11:
  case 12:
#ifdef USE_VM // Nintendo GC Virtual Memory
      value = VMRead32( alignedAddress & 0x1FFFFFC );
#else
      value = READ32LE(((u32 *)&rom[alignedAddress & 0x1FFFFFC]));
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
      default:     value = flashRead(alignedAddress) * 0x01010101; break;
      }
    } else {
      value = flashRead(alignedAddress) * 0x01010101;
    }
    break;
  default:
  unreadable:
    if(cpuDmaHack) {
      value = cpuDmaLast;
    } else {
      if(armState) {
#ifdef USE_VM // Nintendo GC Virtual Memory
        return CPUReadMemoryQuick(reg[15].I);
#else
        return CPUReadMemoryQuickDef(reg[15].I);
#endif
      } else {
#ifdef USE_VM // Nintendo GC Virtual Memory
        return CPUReadHalfWordQuick(reg[15].I) | (CPUReadHalfWordQuick(reg[15].I) << 16);
#else
        return CPUReadHalfWordQuickDef(reg[15].I) | (CPUReadHalfWordQuickDef(reg[15].I) << 16);
#endif
      }
    }
    break;
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

  switch(alignedAddress >> 24) {
  case 0:
    if (reg[15].I >> 24) {
      if(alignedAddress < 0x4000) {
        value = READ16LE(((u16 *)&biosProtected[alignedAddress & 2]));
      } else goto unreadable;
    } else {
      value = READ16LE(((u16 *)&bios[alignedAddress & 0x3FFE]));
    }
    break;
  case 2:
    value = READ16LE(((u16 *)&workRAM[alignedAddress & 0x3FFFE]));
    break;
  case 3:
    value = READ16LE(((u16 *)&internalRAM[alignedAddress & 0x7ffe]));
    break;
  case 4:
    if(LIKELY(alignedAddress < 0x4000400)) {
      u32 ioAddr = alignedAddress & 0x3fe;

      if(ioReadable[ioAddr]) {
        value = READ16LE(((u16 *)&ioMem[ioAddr]));

        // OPTIMIZATION: Consolidate bounds check, leveraging tight variable scope.
        if (UNLIKELY(ioAddr >= 0x100 && ioAddr < 0x110)) {
          if (ioAddr == 0x100 && timer0On)
            value = 0xFFFF - ((timer0Ticks - cpuTotalTicks) >> timer0ClockReload);
          else if (ioAddr == 0x104 && timer1On && !(TM1CNT & 4))
            value = 0xFFFF - ((timer1Ticks - cpuTotalTicks) >> timer1ClockReload);
          else if (ioAddr == 0x108 && timer2On && !(TM2CNT & 4))
            value = 0xFFFF - ((timer2Ticks - cpuTotalTicks) >> timer2ClockReload);
          else if (ioAddr == 0x10C && timer3On && !(TM3CNT & 4))
            value = 0xFFFF - ((timer3Ticks - cpuTotalTicks) >> timer3ClockReload);
        }
      } else if(ioReadable[alignedAddress & 0x3fc]) {
        value = 0;
      } else {
        goto unreadable;
      }
    } else {
      goto unreadable;
    }
    break;
  case 5:
    value = READ16LE(((u16 *)&paletteRAM[alignedAddress & 0x3fe]));
    break;
  case 6:
    alignedAddress = (alignedAddress & 0x1fffe);
    if (UNLIKELY(((DISPCNT & 7) > 2) && ((alignedAddress & 0x1C000) == 0x18000))) {
        value = 0;
        break;
    }
    if ((alignedAddress & 0x18000) == 0x18000)
      alignedAddress &= 0x17fff;
    value = READ16LE(((u16 *)&vram[alignedAddress]));
    break;
  case 7:
    value = READ16LE(((u16 *)&oam[alignedAddress & 0x3fe]));
    break;
  case 8:
    // This is possibly the GPIO port that controls the real time clock,
	// WarioWare Twisted! tilt sensors, rumble, and solar sensors.
	// This function still works if there is no real time clock
	// and does a normal memory read in that case.
    if(UNLIKELY(alignedAddress >= 0x80000c4 && alignedAddress <= 0x80000c8)) {
      value = rtcRead(alignedAddress & 0xFFFFFFE);
      break;
    }
  case 9:
  case 10:
  case 11:
  case 12:
#ifdef USE_VM // Nintendo GC Virtual Memory
    value = VMRead16( alignedAddress & 0x1FFFFFE );
#else
    value = READ16LE(((u16 *)&rom[alignedAddress & 0x1FFFFFE]));
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
      default:     value = flashRead(alignedAddress) * 0x0101; break;
      }
    } else {
      value = flashRead(alignedAddress) * 0x0101;
    }
    break;
  default:
  unreadable:
    if(cpuDmaHack) {
      value = cpuDmaLast & 0xFFFF;
    } else {
      if(armState) {
#ifdef USE_VM // Nintendo GC Virtual Memory
        value = CPUReadHalfWordQuick(reg[15].I + (address & 2));
#else
        value = CPUReadHalfWordQuickDef(reg[15].I + (address & 2));
#endif
        } else {
#ifdef USE_VM // Nintendo GC Virtual Memory
        value = CPUReadHalfWordQuick(reg[15].I);
#else
        value = CPUReadHalfWordQuickDef(reg[15].I);
#endif
      }
    }
    return value;
  }

  // OPTIMIZATION: ROTR 8 branchless equivalent.
  if(UNLIKELY(address & 1)) {
    value = (value >> 8) | (value << 24);
  }

  return value;
}


// OPTIMIZATION: Eliminate useless branch check. The explicit downcast to s16
// handles PowerPC sign extension organically on the returned value.
static inline s16 CPUReadHalfWordSigned(u32 address)
{
  return (s16)CPUReadHalfWord(address);
}


static inline u8 CPUReadByte(u32 address)
{
  switch(address >> 24) {
  case 0:
    if (reg[15].I >> 24) {
      if(address < 0x4000) return biosProtected[address & 3];
      else goto unreadable;
    }
    return bios[address & 0x3FFF];
  case 2:
    return workRAM[address & 0x3FFFF];
  case 3:
    return internalRAM[address & 0x7fff];
  case 4:
    if(LIKELY(address < 0x4000400) && ioReadable[address & 0x3ff])
      return ioMem[address & 0x3ff];
    else goto unreadable;
  case 5:
    return paletteRAM[address & 0x3ff];
  case 6:
    // OPTIMIZATION: Condense unaligned mask early.
    address &= 0x1ffff;
    if (UNLIKELY(((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))) return 0;
    if ((address & 0x18000) == 0x18000) address &= 0x17fff;
    return vram[address];
  case 7:
    return oam[address & 0x3ff];
  case 8:
	// the real time clock doesn't support byte reads, so don't bother checking for it.
  case 9:
  case 10:
  case 11:
  case 12:
#ifdef USE_VM // Nintendo GC Virtual Memory
    return VMRead8( address & 0x1FFFFFF );
#else
    return rom[address & 0x1FFFFFF];
#endif
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
  unreadable:
    if(cpuDmaHack) {
      return cpuDmaLast & 0xFF;
    } else {
      if(armState) {
#ifdef USE_VM // Nintendo GC Virtual Memory
        return CPUReadByteQuick(reg[15].I + (address & 3));
#else
        return CPUReadByteQuickDef(reg[15].I + (address & 3));
#endif
      } else {
#ifdef USE_VM // Nintendo GC Virtual Memory
        return CPUReadByteQuick(reg[15].I + (address & 1));
#else
        return CPUReadByteQuickDef(reg[15].I + (address & 1));
#endif
      }
    }
  }
}

static inline void CPUWriteMemory(u32 address, u32 value)
{
  address &= 0xFFFFFFFC;

  switch(address >> 24) {
  case 0x02:
      WRITE32LE(((u32 *)&workRAM[address & 0x3FFFC]), value);
    break;
  case 0x03:
      WRITE32LE(((u32 *)&internalRAM[address & 0x7ffC]), value);
    break;
  case 0x04:
    if(address < 0x4000400) {
      CPUUpdateRegister((address & 0x3FC), value & 0xFFFF);
      CPUUpdateRegister((address & 0x3FC) + 2, (value >> 16));
    } else goto unwritable;
    break;
  case 0x05:
    if(address < 0x5000400 || (RomIdCode & 0xFFFFFF) != CORVETTE)
      WRITE32LE(((u32 *)&paletteRAM[address & 0x3FC]), value);
    break;
  case 0x06:
    address = (address & 0x1fffc);
    if (((DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
      return;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
      WRITE32LE(((u32 *)&vram[address]), value);
    break;
  case 0x07:
      WRITE32LE(((u32 *)&oam[address & 0x3fc]), value);
    break;
  case 0x0D:
    if(cpuEEPROMEnabled) {
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
    // default
  default:
unwritable:
    break;
  }
}

static inline void CPUWriteHalfWord(u32 address, u16 value)
{
  address &= 0xFFFFFFFE;

  switch(address >> 24) {
  case 2:
      WRITE16LE(((u16 *)&workRAM[address & 0x3FFFE]),value);
    break;
  case 3:
      WRITE16LE(((u16 *)&internalRAM[address & 0x7ffe]), value);
    break;
  case 4:
    if(address < 0x4000400)
      CPUUpdateRegister(address & 0x3fe, value);
    else goto unwritable;
    break;
  case 5:
    if(address < 0x5000400 || (RomIdCode & 0xFFFFFF) != CORVETTE)
      WRITE16LE(((u16 *)&paletteRAM[address & 0x3fe]), value);
    break;
  case 6:
    address = (address & 0x1fffe);
    if (((DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
      return;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
      WRITE16LE(((u16 *)&vram[address]), value);
    break;
  case 7:
      WRITE16LE(((u16 *)&oam[address & 0x3fe]), value);
    break;
  case 8:
  case 9:
    if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8) {
      if(!rtcWrite(address, value))
        goto unwritable;
    } else if(!agbPrintWrite(address, value)) goto unwritable;
    break;
  case 13:
    if(cpuEEPROMEnabled) {
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
    if(address < 0x4000400) {
      switch(address & 0x3FF) {
      case 0x60:
      case 0x61:
      case 0x62:
      case 0x63:
      case 0x64:
      case 0x65:
      case 0x68:
      case 0x69:
      case 0x6c:
      case 0x6d:
      case 0x70:
      case 0x71:
      case 0x72:
      case 0x73:
      case 0x74:
      case 0x75:
      case 0x78:
      case 0x79:
      case 0x7c:
      case 0x7d:
      case 0x80:
      case 0x81:
      case 0x84:
      case 0x85:
      case 0x90:
      case 0x91:
      case 0x92:
      case 0x93:
      case 0x94:
      case 0x95:
      case 0x96:
      case 0x97:
      case 0x98:
      case 0x99:
      case 0x9a:
      case 0x9b:
      case 0x9c:
      case 0x9d:
      case 0x9e:
      case 0x9f:
        soundEvent(address&0xFF, b);
        break;
      case 0x301: // HALTCNT, undocumented
        if(b == 0x80)
          stopState = true;
        holdState = 1;
        holdType = -1;
        cpuNextEvent = cpuTotalTicks;
        break;
      default: // every other register
        u32 lowerBits = address & 0x3fe;
        if(address & 1) {
          CPUUpdateRegister(lowerBits, (READ16LE(&ioMem[lowerBits]) & 0x00FF) | (b << 8));
        } else {
          CPUUpdateRegister(lowerBits, (READ16LE(&ioMem[lowerBits]) & 0xFF00) | b);
        }
      }
      break;
    } else goto unwritable;
    break;
  case 5:
    // no need to switch
    *((u16 *)&paletteRAM[address & 0x3FE]) = (b << 8) | b;
    break;
  case 6:
    address = (address & 0x1fffe);
    if (((DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
      return;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;

    // no need to switch
    // byte writes to OBJ VRAM are ignored
    if ((address) < objTilesAddress[((DISPCNT&7)+1)>>2])
    {
        *((u16 *)&vram[address]) = (b << 8) | b;
    }
    break;
  case 7:
    // no need to switch
    // byte writes to OAM are ignored
    //    *((u16 *)&oam[address & 0x3FE]) = (b << 8) | b;
    break;
  case 13:
    if(cpuEEPROMEnabled) {
      eepromWrite(address, b);
      break;
    }
    goto unwritable;
  case 14:
  case 15:
    if ((saveType != 5) && ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled)) {

      //if(!cpuEEPROMEnabled && (cpuSramEnabled | cpuFlashEnabled)) {

      (*cpuSaveGameFunc)(address, b);
      break;
    }
    // default
  default:
unwritable:
    break;
  }
}

#endif // GBAINLINE_H

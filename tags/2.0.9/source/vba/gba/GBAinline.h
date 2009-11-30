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

#define gid(a,b,c) (a|(b<<8)|(c<<16))
#define CORVETTE		gid('A','V','C')

/*****************************************************************************
 * Nintendo GC Virtual Memory function override
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
 * End of NGC VM override
 ****************************************************************************/

static inline u32 CPUReadMemory(u32 address)
{
#ifdef GBA_LOGGING
  if(address & 3) {
    if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
      log("Unaligned word read: %08x at %08x\n", address, armMode ?
          armNextPC - 4 : armNextPC - 2);
    }
  }
#endif

  u32 value;
  switch(address >> 24) {
  case 0x00:
    if(reg[15].I >> 24) {
      if(address < 0x4000) {
#ifdef GBA_LOGGING
        if(systemVerbose & VERBOSE_ILLEGAL_READ) {
          log("Illegal word read: %08x at %08x\n", address, armMode ?
              armNextPC - 4 : armNextPC - 2);
        }
#endif

        value = READ32LE(((u32 *)&biosProtected));
      }
      else goto unreadable;
    } else
      value = READ32LE(((u32 *)&bios[address & 0x3FFC]));
    break;
  case 0x02:
    value = READ32LE(((u32 *)&workRAM[address & 0x3FFFC]));
    break;
  case 0x03:
    value = READ32LE(((u32 *)&internalRAM[address & 0x7ffC]));
    break;
  case 0x04:
    if((address < 0x4000400) && ioReadable[address & 0x3fc]) {
      if(ioReadable[(address & 0x3fc) + 2])
        value = READ32LE(((u32 *)&ioMem[address & 0x3fC]));
      else
        value = READ16LE(((u16 *)&ioMem[address & 0x3fc]));
    } else goto unreadable;
    break;
  case 0x05:
    value = READ32LE(((u32 *)&paletteRAM[address & 0x3fC]));
    break;
  case 0x06:
    address = (address & 0x1fffc);
    if (((DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
    {
        value = 0;
        break;
    }
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
    value = READ32LE(((u32 *)&vram[address]));
    break;
  case 0x07:
    value = READ32LE(((u32 *)&oam[address & 0x3FC]));
    break;
  case 0x08:
	// Must be cartridge ROM, reading other sensors doesn't allow 32-bit access.
  case 0x09:
  case 0x0A:
  case 0x0B:
  case 0x0C:
#ifdef USE_VM // Nintendo GC Virtual Memory
	  value = VMRead32( address & 0x1FFFFFC );
#else
	  value = READ32LE(((u32 *)&rom[address&0x1FFFFFC]));
#endif
    break;
  case 0x0D:
    if(cpuEEPROMEnabled)
      // no need to swap this
      return eepromRead(address);
    goto unreadable;
  case 0x0E:
	// Yoshi's Universal Gravitation (Topsy Turvy)
	// Koro Koro
    if(cpuEEPROMSensorEnabled) {
      switch(address & 0x00008f00) {
      case 0x8200:
        return systemGetSensorX() & 255;
      case 0x8300:
        return (systemGetSensorX() >> 8)|0x80;
      case 0x8400:
        return systemGetSensorY() & 255;
      case 0x8500:
        return systemGetSensorY() >> 8;
      }
    }
    if(cpuFlashEnabled | cpuSramEnabled)
      // no need to swap this
      return flashRead(address);
    // default
  default:
  unreadable:
#ifdef GBA_LOGGING
    if(systemVerbose & VERBOSE_ILLEGAL_READ) {
      log("Illegal word read: %08x at %08x\n", address, armMode ?
          armNextPC - 4 : armNextPC - 2);
    }
#endif

    if(cpuDmaHack) {
      value = cpuDmaLast;
    } else {
      if(armState) {
#ifdef USE_VM // Nintendo GC Virtual Memory
    	  value = CPUReadMemoryQuick(reg[15].I);
#else
        value = CPUReadMemoryQuickDef(reg[15].I);
#endif
      } else {
#ifdef USE_VM // Nintendo GC Virtual Memory
        value = CPUReadHalfWordQuick(reg[15].I) |
          CPUReadHalfWordQuick(reg[15].I) << 16;
#else
        value = CPUReadHalfWordQuickDef(reg[15].I) |
          CPUReadHalfWordQuickDef(reg[15].I) << 16;
#endif
      }
    }
  }

  if(address & 3) {
#ifdef C_CORE
    int shift = (address & 3) << 3;
    value = (value >> shift) | (value << (32 - shift));
#else
#ifdef __GNUC__
    asm("and $3, %%ecx;"
        "shl $3 ,%%ecx;"
        "ror %%cl, %0"
        : "=r" (value)
        : "r" (value), "c" (address));
#else
    __asm {
      mov ecx, address;
      and ecx, 3;
      shl ecx, 3;
      ror [dword ptr value], cl;
    }
#endif
#endif
  }
  return value;
}

extern u32 myROM[];

static inline u32 CPUReadHalfWord(u32 address)
{
#ifdef GBA_LOGGING
  if(address & 1) {
    if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
      log("Unaligned halfword read: %08x at %08x\n", address, armMode ?
          armNextPC - 4 : armNextPC - 2);
    }
  }
#endif

  u32 value;

  switch(address >> 24) {
  case 0x00:
    if (reg[15].I >> 24) {
      if(address < 0x4000) {
#ifdef GBA_LOGGING
        if(systemVerbose & VERBOSE_ILLEGAL_READ) {
          log("Illegal halfword read: %08x at %08x\n", address, armMode ?
              armNextPC - 4 : armNextPC - 2);
        }
#endif
        value = READ16LE(((u16 *)&biosProtected[address&2]));
      } else goto unreadable;
    } else
      value = READ16LE(((u16 *)&bios[address & 0x3FFE]));
    break;
  case 0x02:
    value = READ16LE(((u16 *)&workRAM[address & 0x3FFFE]));
    break;
  case 0x03:
    value = READ16LE(((u16 *)&internalRAM[address & 0x7ffe]));
    break;
  case 0x04:
    if((address < 0x4000400) && ioReadable[address & 0x3fe])
    {
      value =  READ16LE(((u16 *)&ioMem[address & 0x3fe]));
      if (((address & 0x3fe)>0xFF) && ((address & 0x3fe)<0x10E))
      {
        if (((address & 0x3fe) == 0x100) && timer0On)
          value = 0xFFFF - ((timer0Ticks-cpuTotalTicks) >> timer0ClockReload);
        else
        if (((address & 0x3fe) == 0x104) && timer1On && !(TM1CNT & 4))
          value = 0xFFFF - ((timer1Ticks-cpuTotalTicks) >> timer1ClockReload);
        else
        if (((address & 0x3fe) == 0x108) && timer2On && !(TM2CNT & 4))
          value = 0xFFFF - ((timer2Ticks-cpuTotalTicks) >> timer2ClockReload);
        else
        if (((address & 0x3fe) == 0x10C) && timer3On && !(TM3CNT & 4))
          value = 0xFFFF - ((timer3Ticks-cpuTotalTicks) >> timer3ClockReload);
      }
    }
    else goto unreadable;
    break;
  case 0x05:
    value = READ16LE(((u16 *)&paletteRAM[address & 0x3fe]));
    break;
  case 0x06:
    address = (address & 0x1fffe);
    if (((DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
    {
        value = 0;
        break;
    }
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
    value = READ16LE(((u16 *)&vram[address]));
    break;
  case 0x07:
    value = READ16LE(((u16 *)&oam[address & 0x3fe]));
    break;
  case 0x08:
	// Use existing case statement and faster test for potential speed improvement
	// This is possibly the GPIO port that controls the real time clock,
	// WarioWare Twisted! tilt sensors, rumble, and solar sensors.
    if(address >= 0x80000c4 && address <= 0x80000c8) {
	  // this function still works if there is no real time clock
	  // and does a normal memory read in that case.
      value = rtcRead(address & 0xFFFFFFE);
	  break;
	}
  case 0x09:
  case 0x0A:
  case 0x0B:
  case 0x0C:
#ifdef USE_VM // Nintendo GC Virtual Memory
    	value = VMRead16( address & 0x1FFFFFE );
#else
    	value = READ16LE(((u16 *)&rom[address & 0x1FFFFFE]));
#endif
    break;
  case 0x0D:
    if(cpuEEPROMEnabled)
      // no need to swap this
      return  eepromRead(address);
    goto unreadable;
  case 0x0E:
	// Yoshi's Universal Gravitation (Topsy Turvy)
	// Koro Koro
    if(cpuEEPROMSensorEnabled) {
      switch(address & 0x00008f00) {
      case 0x8200:
        return systemGetSensorX() & 255;
      case 0x8300:
        return (systemGetSensorX() >> 8)|0x80;
      case 0x8400:
        return systemGetSensorY() & 255;
      case 0x8500:
        return systemGetSensorY() >> 8;
      }
    }
    if(cpuFlashEnabled | cpuSramEnabled)
      // no need to swap this
      return flashRead(address);
    // default
  default:
  unreadable:
#ifdef GBA_LOGGING
    if(systemVerbose & VERBOSE_ILLEGAL_READ) {
      log("Illegal halfword read: %08x at %08x\n", address, armMode ?
          armNextPC - 4 : armNextPC - 2);
    }
#endif
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
    break;
  }

  if(address & 1) {
    value = (value >> 8) | (value << 24);
  }

  return value;
}

static inline u16 CPUReadHalfWordSigned(u32 address)
{
  u16 value = CPUReadHalfWord(address);
  if((address & 1))
    value = (s8)value;
  return value;
}

static inline u8 CPUReadByte(u32 address)
{
  switch(address >> 24) {
  case 0x00:
    if (reg[15].I >> 24) {
      if(address < 0x4000) {
#ifdef GBA_LOGGING
        if(systemVerbose & VERBOSE_ILLEGAL_READ) {
          log("Illegal byte read: %08x at %08x\n", address, armMode ?
              armNextPC - 4 : armNextPC - 2);
        }
#endif
        return biosProtected[address & 3];
      } else goto unreadable;
    }
    return bios[address & 0x3FFF];
  case 0x02:
    return workRAM[address & 0x3FFFF];
  case 0x03:
    return internalRAM[address & 0x7fff];
  case 0x04:
    if((address < 0x4000400) && ioReadable[address & 0x3ff])
      return ioMem[address & 0x3ff];
    else goto unreadable;
  case 0x05:
    return paletteRAM[address & 0x3ff];
  case 0x06:
    address = (address & 0x1ffff);
    if (((DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
        return 0;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
    return vram[address];
  case 0x07:
    return oam[address & 0x3ff];
  case 0x08:
	// the real time clock doesn't support byte reads, so don't bother checking for it.
  case 0x09:
  case 0x0A:
  case 0x0B:
  case 0x0C:
#ifdef USE_VM // Nintendo GC Virtual Memory
	return VMRead8( address & 0x1FFFFFF );
#else
    return rom[address & 0x1FFFFFF];
#endif
  case 0x0D:
    if(cpuEEPROMEnabled)
      return eepromRead(address);
    goto unreadable;
  case 0x0E:
	// Yoshi's Universal Gravitation (Topsy Turvy)
	// Koro Koro
    if(cpuEEPROMSensorEnabled) {
      switch(address & 0x00008f00) {
      case 0x8200:
        return systemGetSensorX() & 255;
      case 0x8300:
        return (systemGetSensorX() >> 8)|0x80;
      case 0x8400:
        return systemGetSensorY() & 255;
      case 0x8500:
        return systemGetSensorY() >> 8;
      }
    }
    if(cpuSramEnabled | cpuFlashEnabled)
      return flashRead(address);
    // default
  default:
  unreadable:
#ifdef GBA_LOGGING
    if(systemVerbose & VERBOSE_ILLEGAL_READ) {
      log("Illegal byte read: %08x at %08x\n", address, armMode ?
          armNextPC - 4 : armNextPC - 2);
    }
#endif
    if(cpuDmaHack) {
      return cpuDmaLast & 0xFF;
    } else {
      if(armState) {
#ifdef USE_VM // Nintendo GC Virtual Memory
        return CPUReadByteQuick(reg[15].I+(address & 3));
#else
        return CPUReadByteQuickDef(reg[15].I+(address & 3));
#endif
      } else {
#ifdef USE_VM // Nintendo GC Virtual Memory
        return CPUReadByteQuick(reg[15].I+(address & 1));
#else
        return CPUReadByteQuickDef(reg[15].I+(address & 1));
#endif
      }
    }
    break;
  }
}

static inline void CPUWriteMemory(u32 address, u32 value)
{

#ifdef GBA_LOGGING
  if(address & 3) {
    if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
      log("Unaligned word write: %08x to %08x from %08x\n",
          value,
          address,
          armMode ? armNextPC - 4 : armNextPC - 2);
    }
  }
#endif

  switch(address >> 24) {
  case 0x02:
#ifdef BKPT_SUPPORT
    if(*((u32 *)&freezeWorkRAM[address & 0x3FFFC]))
      cheatsWriteMemory(address & 0x203FFFC,
                        value);
    else
#endif
      WRITE32LE(((u32 *)&workRAM[address & 0x3FFFC]), value);
    break;
  case 0x03:
#ifdef BKPT_SUPPORT
    if(*((u32 *)&freezeInternalRAM[address & 0x7ffc]))
      cheatsWriteMemory(address & 0x3007FFC,
                        value);
    else
#endif
      WRITE32LE(((u32 *)&internalRAM[address & 0x7ffC]), value);
    break;
  case 0x04:
    if(address < 0x4000400) {
      CPUUpdateRegister((address & 0x3FC), value & 0xFFFF);
      CPUUpdateRegister((address & 0x3FC) + 2, (value >> 16));
    } else goto unwritable;
    break;
  case 0x05:
#ifdef BKPT_SUPPORT
    if(*((u32 *)&freezePRAM[address & 0x3fc]))
      cheatsWriteMemory(address & 0x70003FC,
                        value);
    else
#endif
    if(address < 0x5000400 || (RomIdCode & 0xFFFFFF) != CORVETTE)
    WRITE32LE(((u32 *)&paletteRAM[address & 0x3FC]), value);
    break;
  case 0x06:
    address = (address & 0x1fffc);
    if (((DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
        return;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;

#ifdef BKPT_SUPPORT
    if(*((u32 *)&freezeVRAM[address]))
      cheatsWriteMemory(address + 0x06000000, value);
    else
#endif

    WRITE32LE(((u32 *)&vram[address]), value);
    break;
  case 0x07:
#ifdef BKPT_SUPPORT
    if(*((u32 *)&freezeOAM[address & 0x3fc]))
      cheatsWriteMemory(address & 0x70003FC,
                        value);
    else
#endif
    WRITE32LE(((u32 *)&oam[address & 0x3fc]), value);
    break;
  case 0x0D:
    if(cpuEEPROMEnabled) {
      eepromWrite(address, value);
      break;
    }
    goto unwritable;
  case 0x0E:
    if(!eepromInUse | cpuSramEnabled | cpuFlashEnabled) {
      (*cpuSaveGameFunc)(address, (u8)value);
      break;
    }
    // default
  default:
  unwritable:
#ifdef GBA_LOGGING
    if(systemVerbose & VERBOSE_ILLEGAL_WRITE) {
      log("Illegal word write: %08x to %08x from %08x\n",
          value,
          address,
          armMode ? armNextPC - 4 : armNextPC - 2);
    }
#endif
    break;
  }
}

static inline void CPUWriteHalfWord(u32 address, u16 value)
{
#ifdef GBA_LOGGING
  if(address & 1) {
    if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
      log("Unaligned halfword write: %04x to %08x from %08x\n",
          value,
          address,
          armMode ? armNextPC - 4 : armNextPC - 2);
    }
  }
#endif

  switch(address >> 24) {
  case 2:
#ifdef BKPT_SUPPORT
    if(*((u16 *)&freezeWorkRAM[address & 0x3FFFE]))
      cheatsWriteHalfWord(address & 0x203FFFE,
                          value);
    else
#endif
      WRITE16LE(((u16 *)&workRAM[address & 0x3FFFE]),value);
    break;
  case 3:
#ifdef BKPT_SUPPORT
    if(*((u16 *)&freezeInternalRAM[address & 0x7ffe]))
      cheatsWriteHalfWord(address & 0x3007ffe,
                          value);
    else
#endif
      WRITE16LE(((u16 *)&internalRAM[address & 0x7ffe]), value);
    break;
  case 4:
    if(address < 0x4000400)
      CPUUpdateRegister(address & 0x3fe, value);
    else goto unwritable;
    break;
  case 5:
#ifdef BKPT_SUPPORT
    if(*((u16 *)&freezePRAM[address & 0x03fe]))
      cheatsWriteHalfWord(address & 0x70003fe,
                          value);
    else
#endif
    if(address < 0x5000400 || (RomIdCode & 0xFFFFFF) != CORVETTE)
    WRITE16LE(((u16 *)&paletteRAM[address & 0x3fe]), value);
    break;
  case 6:
    address = (address & 0x1fffe);
    if (((DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
        return;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
#ifdef BKPT_SUPPORT
    if(*((u16 *)&freezeVRAM[address]))
      cheatsWriteHalfWord(address + 0x06000000,
                          value);
    else
#endif
    WRITE16LE(((u16 *)&vram[address]), value);
    break;
  case 7:
#ifdef BKPT_SUPPORT
    if(*((u16 *)&freezeOAM[address & 0x03fe]))
      cheatsWriteHalfWord(address & 0x70003fe,
                          value);
    else
#endif
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
    if(!eepromInUse | cpuSramEnabled | cpuFlashEnabled) {
      (*cpuSaveGameFunc)(address, (u8)value);
      break;
    }
    goto unwritable;
  default:
  unwritable:
#ifdef GBA_LOGGING
    if(systemVerbose & VERBOSE_ILLEGAL_WRITE) {
      log("Illegal halfword write: %04x to %08x from %08x\n",
          value,
          address,
          armMode ? armNextPC - 4 : armNextPC - 2);
    }
#endif
    break;
  }
}

static inline void CPUWriteByte(u32 address, u8 b)
{
  switch(address >> 24) {
  case 2:
#ifdef BKPT_SUPPORT
    if(freezeWorkRAM[address & 0x3FFFF])
      cheatsWriteByte(address & 0x203FFFF, b);
    else
#endif
      workRAM[address & 0x3FFFF] = b;
    break;
  case 3:
#ifdef BKPT_SUPPORT
    if(freezeInternalRAM[address & 0x7fff])
      cheatsWriteByte(address & 0x3007fff, b);
    else
#endif
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
#ifdef BKPT_SUPPORT
      if(freezeVRAM[address])
        cheatsWriteByte(address + 0x06000000, b);
      else
#endif
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
    if (!(saveType == 5) && (!eepromInUse | cpuSramEnabled | cpuFlashEnabled)) {

      //if(!cpuEEPROMEnabled && (cpuSramEnabled | cpuFlashEnabled)) {

      (*cpuSaveGameFunc)(address, b);
      break;
    }
    // default
  default:
unwritable:
#ifdef GBA_LOGGING
    if(systemVerbose & VERBOSE_ILLEGAL_WRITE) {
      log("Illegal byte write: %02x to %08x from %08x\n",
        b,
        address,
        armMode ? armNextPC - 4 : armNextPC -2 );
    }
#endif
    break;
  }
}

#endif // GBAINLINE_H

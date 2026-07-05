#ifndef GBACPU_H
#define GBACPU_H

extern int armExecute();
extern int thumbExecute();

# define INSN_REGPARM /*nothing*/
# define LIKELY(x) __builtin_expect(!!(x),1)
# define UNLIKELY(x) __builtin_expect(!!(x),0)

#define UPDATE_REG(address, value)\
  {\
    WRITE16LE(((u16 *)&ioMem[address]),value);\
  }\

#define ARM_PREFETCH \
  {\
    cpuPrefetch[0] = CPUReadMemoryQuick(armNextPC);\
    cpuPrefetch[1] = CPUReadMemoryQuick(armNextPC+4);\
  }

#define THUMB_PREFETCH \
  {\
    cpuPrefetch[0] = CPUReadHalfWordQuick(armNextPC);\
    cpuPrefetch[1] = CPUReadHalfWordQuick(armNextPC+2);\
  }

#define ARM_PREFETCH_NEXT \
  cpuPrefetch[1] = CPUReadMemoryQuick(armNextPC+4);

#define THUMB_PREFETCH_NEXT\
  cpuPrefetch[1] = CPUReadHalfWordQuick(armNextPC+2);


extern int SWITicks;
extern u32 mastercode;
extern bool busPrefetch;
extern bool busPrefetchEnable;
extern u32 busPrefetchCount;
extern int cpuNextEvent;
extern bool holdState;
extern u32 cpuPrefetch[2];
extern int cpuTotalTicks;
extern u8 memoryWait[16];
extern u8 memoryWait32[16];
extern u8 memoryWaitSeq[16];
extern u8 memoryWaitSeq32[16];
extern u8 cpuBitsSet[256];
extern u8 cpuLowestBitSet[256];
extern void CPUSwitchMode(int mode, bool saveState, bool breakLoop);
extern void CPUSwitchMode(int mode, bool saveState);
extern void CPUUpdateCPSR();
extern void CPUUpdateFlags(bool breakLoop);
extern void CPUUpdateFlags();
extern void CPUUndefinedException();
extern void CPUSoftwareInterrupt();
extern void CPUSoftwareInterrupt(int comment);


// Waitstates when accessing data
inline int dataTicksAccess16(u32 address) // DATA 8/16bits NON SEQ
{
  u32 addr = (address >> 24) & 15;
  int value = memoryWait[addr];

  // OPTIMIZATION: Unsigned underflow replaces two comparisons and an OR.
  if (UNLIKELY((u32)(addr - 2) >= 6))
  {
    busPrefetchCount = 0;
    busPrefetch = false;
  }
  else if (busPrefetch)
  {
    // OPTIMIZATION: Branchless ternary replacement (avoids isel/branch stalls).
    // If value == 0, (0 - 1) >> 31 is 1. (0 | 1) = 1.
    // If value > 0, (value - 1) >> 31 is 0. (value | 0) = value.
    u32 waitState = value | ((u32)(value - 1) >> 31);
    busPrefetchCount = ((busPrefetchCount + 1) << waitState) - 1;
  }

  return value;
}

inline int dataTicksAccess32(u32 address) // DATA 32bits NON SEQ
{
  u32 addr = (address >> 24) & 15;
  int value = memoryWait32[addr];

  if (UNLIKELY((u32)(addr - 2) >= 6))
  {
    busPrefetchCount = 0;
    busPrefetch = false;
  }
  else if (busPrefetch)
  {
    u32 waitState = value | ((u32)(value - 1) >> 31);
    busPrefetchCount = ((busPrefetchCount + 1) << waitState) - 1;
  }

  return value;
}

inline int dataTicksAccessSeq16(u32 address)// DATA 8/16bits SEQ
{
  u32 addr = (address >> 24) & 15;
  int value = memoryWaitSeq[addr];

  if (UNLIKELY((u32)(addr - 2) >= 6))
  {
    busPrefetchCount = 0;
    busPrefetch = false;
  }
  else if (busPrefetch)
  {
    u32 waitState = value | ((u32)(value - 1) >> 31);
    busPrefetchCount = ((busPrefetchCount + 1) << waitState) - 1;
  }

  return value;
}

inline int dataTicksAccessSeq32(u32 address)// DATA 32bits SEQ
{
  u32 addr = (address >> 24) & 15;
  int value =  memoryWaitSeq32[addr];

  if (UNLIKELY((u32)(addr - 2) >= 6))
  {
    busPrefetchCount = 0;
    busPrefetch = false;
  }
  else if (busPrefetch)
  {
    u32 waitState = value | ((u32)(value - 1) >> 31);
    busPrefetchCount = ((busPrefetchCount + 1) << waitState) - 1;
  }

  return value;
}


// Waitstates when executing opcode
inline int codeTicksAccess16(u32 address) // THUMB NON SEQ
{
  u32 addr = (address >> 24) & 15;

  // OPTIMIZATION: Unsigned bounds check replaces two comparisons and an AND.
  if (LIKELY((u32)(addr - 0x08) <= 5))
  {
    if (busPrefetchCount & 0x1)
    {
      if (busPrefetchCount & 0x2)
      {
        busPrefetchCount = ((busPrefetchCount & 0xFF) >> 2) | (busPrefetchCount & 0xFFFFFF00);
        return 0;
      }
      busPrefetchCount = ((busPrefetchCount & 0xFF) >> 1) | (busPrefetchCount & 0xFFFFFF00);
      return memoryWaitSeq[addr] - 1;
    }
    else
    {
      busPrefetchCount = 0;
      return memoryWait[addr];
    }
  }
  else
  {
    busPrefetchCount = 0;
    return memoryWait[addr];
  }
}

inline int codeTicksAccess32(u32 address) // ARM NON SEQ
{
  u32 addr = (address >> 24) & 15;

  if (LIKELY((u32)(addr - 0x08) <= 5))
  {
    if (busPrefetchCount & 0x1)
    {
      if (busPrefetchCount & 0x2)
      {
        busPrefetchCount = ((busPrefetchCount & 0xFF) >> 2) | (busPrefetchCount & 0xFFFFFF00);
        return 0;
      }
      busPrefetchCount = ((busPrefetchCount & 0xFF) >> 1) | (busPrefetchCount & 0xFFFFFF00);
      return memoryWaitSeq[addr] - 1;
    }
    else
    {
      busPrefetchCount = 0;
      return memoryWait32[addr];
    }
  }
  else
  {
    busPrefetchCount = 0;
    return memoryWait32[addr];
  }
}

inline int codeTicksAccessSeq16(u32 address) // THUMB SEQ
{
  u32 addr = (address >> 24) & 15;

  if (LIKELY((u32)(addr - 0x08) <= 5))
  {
    if (busPrefetchCount & 0x1)
    {
      busPrefetchCount = ((busPrefetchCount & 0xFF) >> 1) | (busPrefetchCount & 0xFFFFFF00);
      return 0;
    }
    else if (busPrefetchCount > 0xFF)
    {
      busPrefetchCount = 0;
      return memoryWait[addr];
    }
    else
      return memoryWaitSeq[addr];
  }
  else
  {
    busPrefetchCount = 0;
    return memoryWaitSeq[addr];
  }
}

inline int codeTicksAccessSeq32(u32 address) // ARM SEQ
{
  u32 addr = (address >> 24) & 15;

  if (LIKELY((u32)(addr - 0x08) <= 5))
  {
    if (busPrefetchCount & 0x1)
    {
      if (busPrefetchCount & 0x2)
      {
        busPrefetchCount = ((busPrefetchCount & 0xFF) >> 2) | (busPrefetchCount & 0xFFFFFF00);
        return 0;
      }
      busPrefetchCount = ((busPrefetchCount & 0xFF) >> 1) | (busPrefetchCount & 0xFFFFFF00);
      return memoryWaitSeq[addr];
    }
    else if (busPrefetchCount > 0xFF)
    {
      busPrefetchCount = 0;
      return memoryWait32[addr];
    }
    else
      return memoryWaitSeq32[addr];
  }
  else
  {
    return memoryWaitSeq32[addr];
  }
}

// Emulates the Cheat System (m) code
inline void cpuMasterCodeCheck()
{
  if((mastercode) && (mastercode == armNextPC))
  {
    u32 joy = 0;
    if(systemReadJoypads())
      joy = systemReadJoypad(-1);
    u32 ext = (joy >> 10);
    cpuTotalTicks += cheatsCheckKeys(P1^0x3FF, ext);
  }
}

#endif // GBACPU_H

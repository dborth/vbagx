#ifndef GBACPU_H
#define GBACPU_H

extern int armExecute();
extern int thumbExecute();

# define INSN_REGPARM /*nothing*/
# define LIKELY(x) __builtin_expect(!!(x),1)
# define UNLIKELY(x) __builtin_expect(!!(x),0)

// Hardware Prefetch helper (pulls host address into L1 D-Cache asynchronously)
// Safely resolves the O(1) pointer to prevent Data Storage Interrupts (DSI) on unmapped banks.
#define GBA_DCBT(addr) \
    do { \
        u8 _bank = (addr) >> 24; \
        u8* _page = gbaReadPagePtrs[_bank]; \
        if (LIKELY(_page != NULL)) { \
            __builtin_prefetch(_page + ((addr) & gbaReadPageMasks[_bank]), 0, 0); \
        } \
    } while(0)

#define ARM_PREFETCH \
    do { \
        cpuPrefetch[0] = CPUReadMemory(armNextPC);\
        cpuPrefetch[1] = CPUReadMemory(armNextPC+4);\
        GBA_DCBT(armNextPC + 32);\
    } while(0)

#define THUMB_PREFETCH \
    do { \
        cpuPrefetch[0] = CPUReadHalfWord(armNextPC);\
        cpuPrefetch[1] = CPUReadHalfWord(armNextPC+2);\
        GBA_DCBT(armNextPC + 32);\
    } while(0)

#define ARM_PREFETCH_NEXT \
    do { \
        cpuPrefetch[1] = CPUReadMemory(armNextPC+4);\
        GBA_DCBT(armNextPC + 32);\
    } while(0)

#define THUMB_PREFETCH_NEXT \
    do { \
        cpuPrefetch[1] = CPUReadHalfWord(armNextPC+2);\
        GBA_DCBT(armNextPC + 32);\
    } while(0)

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
      // 1-cycle bitwise resolution: shift is either 1 or 2
      u32 shift = 1 + ((busPrefetchCount >> 1) & 1);
      busPrefetchCount = ((busPrefetchCount & 0xFF) >> shift) | (busPrefetchCount & 0xFFFFFF00);

      // If shift == 1, mask = 0xFFFFFFFF. If shift == 2, mask = 0.
      return (memoryWaitSeq[addr] - 1) & ((s32)(shift - 2) >> 31);
    }
  }
  busPrefetchCount = 0;
  return memoryWait[addr];
}

inline int codeTicksAccess32(u32 address) // ARM NON SEQ
{
  u32 addr = (address >> 24) & 15;
  if (LIKELY((u32)(addr - 0x08) <= 5))
  {
    if (busPrefetchCount & 0x1)
    {
      u32 shift = 1 + ((busPrefetchCount >> 1) & 1);
      busPrefetchCount = ((busPrefetchCount & 0xFF) >> shift) | (busPrefetchCount & 0xFFFFFF00);
      return (memoryWaitSeq[addr] - 1) & ((s32)(shift - 2) >> 31);
    }
  }
  busPrefetchCount = 0;
  return memoryWait32[addr];
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
      u32 shift = 1 + ((busPrefetchCount >> 1) & 1);
      busPrefetchCount = ((busPrefetchCount & 0xFF) >> shift) | (busPrefetchCount & 0xFFFFFF00);
      return memoryWaitSeq[addr] & ((s32)(shift - 2) >> 31);
    }
    else if (busPrefetchCount > 0xFF)
    {
      busPrefetchCount = 0;
      return memoryWait32[addr];
    }
    return memoryWaitSeq32[addr];
  }
  return memoryWaitSeq32[addr];
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

#ifndef PORT_H
#define PORT_H

#include "Types.h"

// swaps a 16-bit value
static inline u16 swap16(u16 v)
{
  return (v<<8)|(v>>8);
}

// swaps a 32-bit value
static inline u32 swap32(u32 v)
{
  return (v<<24)|((v<<8)&0xff0000)|((v>>8)&0xff00)|(v>>24);
}

#define READ16LE(base) \
  ({ unsigned short lhbrxResult;       \
     __asm__ ("lhbrx %0, 0, %1" : "=r" (lhbrxResult) : "r" (base) : "memory"); \
      lhbrxResult; })

#define READ32LE(base) \
  ({ unsigned long lwbrxResult; \
     __asm__ ("lwbrx %0, 0, %1" : "=r" (lwbrxResult) : "r" (base) : "memory"); \
      lwbrxResult; })

#define WRITE16LE(base, value) \
  __asm__ ("sthbrx %0, 0, %1" : : "r" (value), "r" (base) : "memory")

#define WRITE32LE(base, value) \
  __asm__ ("stwbrx %0, 0, %1" : : "r" (value), "r" (base) : "memory")

#endif // PORT_H

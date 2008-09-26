/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * vmmem.h
 *
 * GameBoy Advance Virtual Memory Paging
 ***************************************************************************/

#ifndef __VBAVMHDR__
#define __VBAVMHDR__

int VMCPULoadROM(int method);
u32 VMRead32( u32 address );
u16 VMRead16( u32 address );
u8 VMRead8( u32 address );

#endif


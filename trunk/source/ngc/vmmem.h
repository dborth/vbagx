/****************************************************************************
* VisualBoyAdvance 1.7.2
*
* GameBoy Advance Virtual Memory Paging
****************************************************************************/
#ifndef __VBAVMHDR__
#define __VBAVMHDR__

int VMCPULoadROM( char *filename );
u32 VMRead32( u32 address );
u16 VMRead16( u32 address );
u8 VMRead8( u32 address );

#endif


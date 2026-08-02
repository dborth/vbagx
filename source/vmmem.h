/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth September 2008
 *
 * vmmem.h
 *
 * GameBoy Advance Virtual Memory Paging
 ***************************************************************************/

#ifndef _VMMEM_H_
#define _VMMEM_H_

#ifdef USE_VM
int VMGBAROMLoad();
void VMClose();
u32 VMRead32( u32 address );
u16 VMRead16( u32 address );
u8 VMRead8( u32 address );
#endif

#endif

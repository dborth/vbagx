/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * vmmem.cpp
 *
 * GameBoy Advance Virtual Memory Paging
 ***************************************************************************/

#ifdef HW_RVL
#include "sdfileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <fat.h>
#include <sys/dir.h>

#include "agb/GBA.h"
#include "Globals.h"
#include "Util.h"
#include "Port.h"

#include "vba.h"
#include "smbop.h"
#include "fileop.h"
#include "dvd.h"
#include "menudraw.h"
#include "filesel.h"

extern "C" {
#include "tbtime.h"
}

#define MEM_BAD	0xff
#define MEM_VM  0x01
#define MEM_UN  0x80

unsigned int MEM2Storage = 0x91000000;

static char *gbabase = NULL;
static u32 GBAROMSize = 0;

/**
 * GBA Memory
 */
#define WORKRAM		0x40000
#define BIOS		0x4000
#define INTERNALRAM	0x8000
#define PALETTERAM	0x400
#define VRAM		0x20000
#define OAM		0x400
#define PIX		(4 * 241 * 162)
#define IOMEM		0x400
#define GBATOTAL	(WORKRAM + BIOS + INTERNALRAM + PALETTERAM + \
			 VRAM + OAM + PIX + IOMEM )

extern void CPUUpdateRenderBuffers(bool force);

/****************************************************************************
* VMAllocGBA
*
* Allocate the memory required for GBA.
****************************************************************************/
static void VMAllocGBA( void )
{
  gbabase = (char *)memalign(32, GBATOTAL);
  memset(gbabase, 0, GBATOTAL);

  /* Assign to internal GBA variables */
  workRAM = (u8 *)gbabase;
  bios = (u8 *)(gbabase + WORKRAM);
  internalRAM = (u8 *)(bios + BIOS);
  paletteRAM = (u8 *)(internalRAM + INTERNALRAM);
  vram = (u8 *)(paletteRAM + PALETTERAM);
  oam = (u8 *)(vram + VRAM);
  pix = (u8 *)(oam + OAM);
  ioMem = (u8 *)(pix + PIX);
}


/****************************************************************************
* VMClose
****************************************************************************/
static void VMClose( void )
{
/*  if ( rombase != NULL )
    free(rombase);
*/
  if ( gbabase != NULL )
    free(gbabase);

  gbabase = NULL;
}

/****************************************************************************
* VMCPULoadROM
*
* MEM2 version of GBA CPULoadROM
****************************************************************************/

int VMCPULoadROM(int method)
{
	VMClose();
	VMAllocGBA();
	GBAROMSize = 0;
	rom = (u8 *)MEM2Storage;

	if(method == METHOD_AUTO)
		method = autoLoadMethod();

	switch (method)
	{
		case METHOD_SD:
		case METHOD_USB:
		GBAROMSize = LoadFATFile ((char *)rom);
		break;

		case METHOD_DVD:
		GBAROMSize = LoadDVDFile ((unsigned char *)rom);
		break;

		case METHOD_SMB:
		GBAROMSize = LoadSMBFile ((char *)rom);
		break;
	}

	if(GBAROMSize)
		CPUUpdateRenderBuffers( true );
	else
		VMClose();

	return GBAROMSize;
}


/****************************************************************************
* VMRead32
*
* Return a 32bit value
****************************************************************************/
u32 VMRead32( u32 address )
{
  u32 badaddress;

  if ( address >= GBAROMSize )
  {
    badaddress = ( ( ( address >> 1 ) & 0xffff ) << 16 ) | ( ( ( address + 2 ) >> 1 ) & 0xffff );
    return badaddress;
  }

  return READ32LE((rom + address));


}

/****************************************************************************
* VMRead16
*
* Return a 16bit value
****************************************************************************/
u16 VMRead16( u32 address )
{
  if ( address >= GBAROMSize )
  {
    return ( address >> 1 ) & 0xffff;
  }

  return READ16LE((rom + address));

}

/****************************************************************************
* VMRead8
*
* Return 8bit value
****************************************************************************/
u8 VMRead8( u32 address )
{

  if ( address >= GBAROMSize )
  {
    return ( address >> 1 ) & 0xff;
  }


  return (u8)rom[address];

}
#else

#include "sdfileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "agb/GBA.h"
#include "Globals.h"
#include "Util.h"
#include "Port.h"

#include "menudraw.h"

extern "C" {
#include "tbtime.h"
}

/** Globals **/
extern u32 loadtimeradjust;

#define MEM_BAD	0xff
#define MEM_VM  0x01
#define MEM_UN  0x80

/** Setup VM to use small 16kb windows **/
#define VMSHIFTBITS 14
#define VMSHIFTMASK 0x3FFF
#define MAXGBAROM ( 32 * 1024 * 1024 )
#define MAXROM  (4 * 1024 * 1024)
#define MAXVMPAGE ( MAXGBAROM >> VMSHIFTBITS )
#define MAXVMMASK ( ( MAXROM >> VMSHIFTBITS ) - 1 )

typedef struct
  {
    char *pageptr;
    int pagetype;
    int pageno;
  }
VMPAGE;

static VMPAGE vmpage[MAXVMPAGE];
static int vmpageno = 0;
static char *rombase = NULL;
static char *gbabase = NULL;
static FILE* romfile = NULL;
static int useVM = 0;
static u32 GBAROMSize = 0;
static char romfilename[1024];

/**
 * GBA Memory
 */
#define WORKRAM		0x40000
#define BIOS		0x4000
#define INTERNALRAM	0x8000
#define PALETTERAM	0x400
#define VRAM		0x20000
#define OAM		0x400
#define PIX		(4 * 241 * 162)
#define IOMEM		0x400
#define GBATOTAL	(WORKRAM + BIOS + INTERNALRAM + PALETTERAM + \
			 VRAM + OAM + PIX + IOMEM )

extern void CPUUpdateRenderBuffers(bool force);

/****************************************************************************
* VMAllocGBA
*
* Allocate the memory required for GBA.
****************************************************************************/
static void VMAllocGBA( void )
{
  gbabase = (char *)memalign(32, GBATOTAL);
  memset(gbabase, 0, GBATOTAL);

  /* Assign to internal GBA variables */
  workRAM = (u8 *)gbabase;
  bios = (u8 *)(gbabase + WORKRAM);
  internalRAM = (u8 *)(bios + BIOS);
  paletteRAM = (u8 *)(internalRAM + INTERNALRAM);
  vram = (u8 *)(paletteRAM + PALETTERAM);
  oam = (u8 *)(vram + VRAM);
  pix = (u8 *)(oam + OAM);
  ioMem = (u8 *)(pix + PIX);
}

/****************************************************************************
* VMFindFree
*
* Look for a free page in the VM block. If none found, do a round-robin
****************************************************************************/
static void VMFindFree( void )
{
  int i;

  vmpageno++;
  vmpageno &= MAXVMMASK;
  if ( vmpageno == 0 ) vmpageno++;

  for ( i = 1; i < MAXVMPAGE; i++ )
    {
      /** Remove any other pointer to this vmpage **/
      if ( vmpage[i].pageno == vmpageno )
        {
          vmpage[i].pageptr = NULL;
          vmpage[i].pagetype = MEM_UN;
          vmpage[i].pageno = -1;
          break;
        }
    }
}

/****************************************************************************
* VMAllocate
*
* Allocate a VM page
****************************************************************************/
static void VMAllocate( int pageid )
{
  VMFindFree();
  vmpage[pageid].pageptr = rombase + ( vmpageno << VMSHIFTBITS );
  vmpage[pageid].pagetype = MEM_VM;
  vmpage[pageid].pageno = vmpageno;

}

/****************************************************************************
* VMInit
*
* Set everything to default
****************************************************************************/
static void VMInit( void )
{
  int i;

  /** Clear down pointers **/
  memset(&vmpage, 0, sizeof(VMPAGE) * MAXVMPAGE);
  for ( i = 0; i < MAXVMPAGE; i++ )
    {
      vmpage[i].pageno = -1;
      vmpage[i].pagetype = MEM_UN;
    }

  /** Allocate physical **/
  if ( rombase == NULL )
    rombase = (char *)memalign(32, MAXROM);

  vmpageno = 0;
  rom = (u8 *)rombase;
}

/****************************************************************************
* VMClose
****************************************************************************/
static void VMClose( void )
{
  if ( rombase != NULL )
    free(rombase);

  if ( gbabase != NULL )
    free(gbabase);

  if ( romfile != NULL )
    gen_fclose(romfile);

  rombase = gbabase = NULL;
  romfile = NULL;

}

/****************************************************************************
* VMCPULoadROM
*
* VM version of GBA CPULoadROM
****************************************************************************/
int VMCPULoadROM( char *filename )
{
  int res;
  char msg[512];

  /** Fix VM **/
  VMClose();
  VMInit();
  VMAllocGBA();

  loadtimeradjust = useVM = GBAROMSize = 0;

  //printf("Filename %s\n", filename);

  romfile = gen_fopen(filename, "rb");
  if ( romfile == NULL )
    {
	  WaitPrompt((char*) "Error opening file!");
      VMClose();
      return 0;
    }

   // printf("ROM Size %d\n", romfile->fsize);

  /* Always use VM, regardless of ROM size */
      res = gen_fread(rom, 1, (1 << VMSHIFTBITS), romfile);
      if ( res != (1 << VMSHIFTBITS ) )
      {
		sprintf(msg, "Error reading file! %i \n",res);
		WaitPrompt(msg);
        VMClose();
        return 0;
      }

	fseek(romfile, 0, SEEK_END);
	GBAROMSize = ftell(romfile);
	fseek(romfile, 0, SEEK_SET);
      vmpageno = 0;
      vmpage[0].pageptr = rombase;
      vmpage[0].pageno = 0;
      vmpage[0].pagetype = MEM_VM;
      useVM = 1;

  strcpy( romfilename, filename );

  CPUUpdateRenderBuffers( true );

  return 1;
}

/****************************************************************************
* GBA Memory Read Routines
****************************************************************************/
/****************************************************************************
* VMNewPage
****************************************************************************/
static void VMNewPage( int pageid )
{
  int res;
  tb_t start,end;
  char msg[512];

  mftb(&start);

  res = gen_fseek( romfile, pageid << VMSHIFTBITS, SEEK_SET );
  if ( ! res )
    {
      sprintf(msg, "Seek error! - Offset %08x %d\n", pageid << VMSHIFTBITS, res);
      WaitPrompt(msg);
      VMClose();
      return;
    }

  VMAllocate( pageid );

  res = gen_fread( vmpage[pageid].pageptr, 1, 1 << VMSHIFTBITS, romfile );
  if ( res != ( 1 << VMSHIFTBITS ) )
    {
      sprintf(msg, "Error reading! %d bytes only\n", res);
      WaitPrompt(msg);
      VMClose();
      return;
    }

   mftb(&end);

   loadtimeradjust += tb_diff_msec(&end, &start);

#if 0
  if ( pageid == 0x1FE )
    {
      vmpage[pageid].pageptr[0x209C] = 0xDF;
      vmpage[pageid].pageptr[0x209D] = 0xFA;
      vmpage[pageid].pageptr[0x209E] = 0x47;
      vmpage[pageid].pageptr[0x209F] = 0x70;
    }

  printf("VMNP : %02x %04x %08x [%02x%02x%02x%02x] [%02x%02x%02x%02x]\n", vmpageno, pageid,
         (u32)(vmpage[pageid].pageptr - rombase), vmpage[pageid].pageptr[0], vmpage[pageid].pageptr[1],
         vmpage[pageid].pageptr[2], vmpage[pageid].pageptr[3],
         vmpage[pageid].pageptr[0xfffc], vmpage[pageid].pageptr[0xfffd],
         vmpage[pageid].pageptr[0xfffe], vmpage[pageid].pageptr[0xffff]	);
#endif

}

/****************************************************************************
* VMRead32
*
* Return a 32bit value
****************************************************************************/
u32 VMRead32( u32 address )
{
  int pageid;
  u32 badaddress;
  char msg[512];
  //printf("VM32 : Request %08x\n", address);

  if ( address >= GBAROMSize )
  {
    badaddress = ( ( ( address >> 1 ) & 0xffff ) << 16 ) | ( ( ( address + 2 ) >> 1 ) & 0xffff );
    return badaddress;
  }

  if ( !useVM )
    return READ32LE((rom + address));

  /** Use VM **/
  pageid = address >> VMSHIFTBITS;

  switch( vmpage[pageid].pagetype )
  {
	case MEM_UN:
		VMNewPage(pageid);

	case MEM_VM:
		return READ32LE( vmpage[pageid].pageptr + ( address & VMSHIFTMASK ) );

	default:
		sprintf(msg, "VM32 : Unknown page type! (%d) [%d]", vmpage[pageid].pagetype, pageid);
		WaitPrompt(msg);
		VMClose();
        return 0;
  }

  /* Can never get here ... but stops gcc bitchin' */
  return 0;

}

/****************************************************************************
* VMRead16
*
* Return a 16bit value
****************************************************************************/
u16 VMRead16( u32 address )
{
  int pageid;

  //printf("VM16 : Request %08x\n", address);

  if ( address >= GBAROMSize )
  {
    return ( address >> 1 ) & 0xffff;
  }

  if ( !useVM )
    return READ16LE((rom + address));

  pageid = address >> VMSHIFTBITS;

  switch( vmpage[pageid].pagetype )
  {
	case MEM_UN:
		VMNewPage(pageid);

	case MEM_VM:
		return READ16LE( vmpage[pageid].pageptr + ( address & VMSHIFTMASK ) );

	default:
		WaitPrompt((char*) "VM16 : Unknown page type!");
  		VMClose();
        return 0;
  }

  /* Can never get here ... but stops gcc bitchin' */
  return 0;

}

/****************************************************************************
* VMRead8
*
* Return 8bit value
****************************************************************************/
u8 VMRead8( u32 address )
{
  int pageid;

  //printf("VM8 : Request %08x\n", address);

  if ( address >= GBAROMSize )
  {
    return ( address >> 1 ) & 0xff;
  }

  if ( !useVM )
    return (u8)rom[address];

  pageid = address >> VMSHIFTBITS;

  switch( vmpage[pageid].pagetype )
  {
	case MEM_UN:
		VMNewPage(pageid);

	case MEM_VM:
	    	return (u8)vmpage[pageid].pageptr[ (address & VMSHIFTMASK) ];

	default:
		WaitPrompt((char*) "VM8 : Unknown page type!");
  		VMClose();
        return 0;
  }

  /* Can never get here ... but stops gcc bitchin' */
  return 0;

}

#endif


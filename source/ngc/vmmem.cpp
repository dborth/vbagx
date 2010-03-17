/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * vmmem.cpp
 *
 * GameBoy Advance Virtual Memory Paging
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <fat.h>
#include <sys/dir.h>

#include "gba/GBA.h"
#include "gba/Globals.h"
#include "Util.h"
#include "common/Port.h"

#include "vba.h"
#include "fileop.h"
#include "menu.h"
#include "filebrowser.h"
#include "gcunzip.h"

#define MEM_BAD	0xff
#define MEM_VM  0x01
#define MEM_UN  0x80

int GBAROMSize = 0;

#ifdef USE_VM

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
static FILE* romfile = NULL;
static char *rombase = NULL;
#endif

extern void CPUUpdateRenderBuffers(bool force);

/****************************************************************************
* VMClose
****************************************************************************/
void VMClose()
{
	if(vram != NULL)
	{
		free(vram);
		vram = NULL;
	}

	if(paletteRAM != NULL)
	{
		free(paletteRAM);
		paletteRAM = NULL;
	}

	if(internalRAM != NULL)
	{
		free(internalRAM);
		internalRAM = NULL;
	}

	if(workRAM != NULL)
	{
		free(workRAM);
		workRAM = NULL;
	}

	if(bios != NULL)
	{
		free(bios);
		bios = NULL;
	}

	if(pix != NULL)
	{
		free(pix);
		pix = NULL;
	}

	if(oam != NULL)
	{
		free(oam);
		oam = NULL;
	}

	if(ioMem != NULL)
	{
		free(ioMem);
		ioMem = NULL;
	}

	#ifdef USE_VM
	if (rombase != NULL)
	{
		free(rombase);
		rombase = NULL;
	}
	#endif
}

/****************************************************************************
* VMAllocGBA
*
* Allocate the memory required for GBA.
****************************************************************************/
static void VMAllocGBA( void )
{
	workRAM = (u8 *)calloc(1, 0x40000);
	bios = (u8 *)calloc(1,0x4000);
	internalRAM = (u8 *)calloc(1,0x8000);
	paletteRAM = (u8 *)calloc(1,0x400);
	vram = (u8 *)calloc(1, 0x20000);
	oam = (u8 *)calloc(1, 0x400);
	pix = (u8 *)calloc(1, 4 * 241 * 162);
	ioMem = (u8 *)calloc(1, 0x400);

	if(workRAM == NULL || bios == NULL || internalRAM == NULL ||
		paletteRAM == NULL || vram == NULL || oam == NULL ||
		pix == NULL || ioMem == NULL)
	{
		ErrorPrompt("Out of memory!");
		VMClose();
	}
}

#ifndef USE_VM
/****************************************************************************
* VMCPULoadROM
*
* MEM2 version of GBA CPULoadROM
****************************************************************************/

bool VMCPULoadROM()
{
	VMClose();
	VMAllocGBA();
	GBAROMSize = 0;

	if(!inSz)
	{
		char filepath[1024];

		if(!MakeFilePath(filepath, FILE_ROM))
			return false;

		GBAROMSize = LoadFile ((char *)rom, filepath, browserList[browser.selIndex].length, NOTSILENT);
	}
	else
	{
		GBAROMSize = LoadSzFile(szpath, (unsigned char *)rom);
	}

	if(GBAROMSize)
	{
		flashInit();
		eepromInit();
		CPUUpdateRenderBuffers( true );
		return true;
	}
	else
	{
		VMClose();
		return false;
	}
}
#else

/****************************************************************************
* VMFindFree
*
* Look for a free page in the VM block. If none found, do a round-robin
****************************************************************************/
static void VMFindFree( void )
{
	++vmpageno;
	vmpageno &= MAXVMMASK;
	if ( vmpageno == 0 ) ++vmpageno;

	for (unsigned i = 1; i < MAXVMPAGE; ++i )
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
	/** Clear down pointers **/
	memset(&vmpage, 0, sizeof(VMPAGE) * MAXVMPAGE);

	if(MAXVMPAGE % 4 == 0)
	{
		for (unsigned i =0 ; i < MAXVMPAGE; i+=4 )
		{
			vmpage[i  ].pageno = -1;
			vmpage[i  ].pagetype = MEM_UN;
			vmpage[i+1].pageno = -1;
			vmpage[i+1].pagetype = MEM_UN;
			vmpage[i+2].pageno = -1;
			vmpage[i+2].pagetype = MEM_UN;
			vmpage[i+3].pageno = -1;
			vmpage[i+3].pagetype = MEM_UN;
		}
	}
	else
	{
		for (unsigned i =0 ; i < MAXVMPAGE; ++i )
		{
			vmpage[i].pageno = -1;
			vmpage[i].pagetype = MEM_UN;
		}
	}

	/** Allocate physical **/
	if ( rombase == NULL )
		rombase = (char *)memalign(32, MAXROM);

	vmpageno = 0;
	rom = (u8 *)rombase;
}

/****************************************************************************
* VMCPULoadROM
*
* VM version of GBA CPULoadROM
****************************************************************************/

int VMCPULoadROM()
{
	int res;
	char filepath[MAXPATHLEN];

	if(!MakeFilePath(filepath, FILE_ROM))
		return 0;

	// loading compressed files via VM is not supported
	if(!utilIsGBAImage(filepath))
	{
		ErrorPrompt("Compressed GBA files are not supported!");
		return 0;
	}

	if (romfile != NULL)
		fclose(romfile);

	romfile = fopen(filepath, "rb");

	if (romfile == NULL)
	{
		ErrorPrompt("Error opening file!");
		return 0;
	}

	/** Fix VM **/
	VMClose();
	VMInit();
	VMAllocGBA();

	GBAROMSize = 0;

	res = fread(rom, 1, (1 << VMSHIFTBITS), romfile);
	if ( res != (1 << VMSHIFTBITS ) )
	{
		ErrorPrompt("Error reading file!");
		VMClose();
		return 0;
	}

	fseeko(romfile,0,SEEK_END);
	GBAROMSize = ftello(romfile);

	vmpageno = 0;
	vmpage[0].pageptr = rombase;
	vmpage[0].pageno = 0;
	vmpage[0].pagetype = MEM_VM;

	flashInit();
	eepromInit();
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
	int res = fseek( romfile, pageid << VMSHIFTBITS, SEEK_SET );

	if (res) // fseek returns non-zero on a failure
	{
		ErrorPrompt("Seek error!");
		VMClose();
		ExitApp();
	}

	VMAllocate( pageid );

	res = fread( vmpage[pageid].pageptr, 1, 1 << VMSHIFTBITS, romfile );
}

/****************************************************************************
 * VMRead32
 *
 * Return a 32bit value
 ****************************************************************************/
u32 VMRead32( u32 address )
{
	if ( address >= (u32)GBAROMSize )
	{
		return u32(( ( ( address >> 1 ) & 0xffff ) << 16 ) | ( ( ( address + 2 ) >> 1 ) & 0xffff ));
	}

	int pageid = address >> VMSHIFTBITS;

	switch( vmpage[pageid].pagetype )
	{
		case MEM_UN:
		VMNewPage(pageid);

		case MEM_VM:
		return READ32LE( vmpage[pageid].pageptr + ( address & VMSHIFTMASK ) );

		default:
		ErrorPrompt("VM32: Unknown page type!");
		VMClose();
		ExitApp();
		return 0;
	}
}

/****************************************************************************
 * VMRead16
 *
 * Return a 16bit value
 ****************************************************************************/
u16 VMRead16( u32 address )
{
	if ( address >= (u32)GBAROMSize )
	{
		return ( address >> 1 ) & 0xffff;
	}

	int pageid = address >> VMSHIFTBITS;

	switch( vmpage[pageid].pagetype )
	{
		case MEM_UN:
		VMNewPage(pageid);

		case MEM_VM:
		return READ16LE( vmpage[pageid].pageptr + ( address & VMSHIFTMASK ) );

		default:
		ErrorPrompt("VM16: Unknown page type!");
		VMClose();
		ExitApp();
		return 0;
	}
}

/****************************************************************************
 * VMRead8
 *
 * Return 8bit value
 ****************************************************************************/
u8 VMRead8( u32 address )
{
	if ( address >= (u32)GBAROMSize )
	{
		return ( address >> 1 ) & 0xff;
	}

	int pageid = address >> VMSHIFTBITS;

	switch( vmpage[pageid].pagetype )
	{
		case MEM_UN:
		VMNewPage(pageid);

		case MEM_VM:
		return (u8)vmpage[pageid].pageptr[ (address & VMSHIFTMASK) ];

		default:
		ErrorPrompt("VM8: Unknown page type!");
		VMClose();
		ExitApp();
		return 0;
	}
}

#endif

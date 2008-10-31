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
#include "gcunzip.h"

extern "C" {
#include "tbtime.h"
}

#define MEM_BAD	0xff
#define MEM_VM  0x01
#define MEM_UN  0x80

unsigned int MEM2Storage = 0x91000000;

int GBAROMSize = 0;

#ifdef USE_VM
//extern u32 loadtimeradjust;

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
	if (romfile != NULL)
	{
		fclose(romfile);
		romfile = NULL;
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
		WaitPrompt((char *)"Out of memory!");
		VMClose();
	}
}

#ifndef USE_VM
/****************************************************************************
* VMCPULoadROM
*
* MEM2 version of GBA CPULoadROM
****************************************************************************/

bool VMCPULoadROM(int method)
{
	VMClose();
	VMAllocGBA();
	GBAROMSize = 0;
	rom = (u8 *)MEM2Storage;

	switch (method)
	{
		case METHOD_SD:
		case METHOD_USB:
			if(inSz)
				GBAROMSize = LoadFATSzFile(szpath, (unsigned char *)rom);
			else
				GBAROMSize = LoadFATFile((char *)rom, filelist[selection].length);
			break;

		case METHOD_DVD:
			if(inSz)
				GBAROMSize = SzExtractFile(filelist[selection].offset, (unsigned char *)rom);
			else
				GBAROMSize = LoadDVDFile((unsigned char *)rom, filelist[selection].length);
			break;

		case METHOD_SMB:
			if(inSz)
				GBAROMSize = LoadSMBSzFile(szpath, (unsigned char *)rom);
			else
				GBAROMSize = LoadSMBFile((char *)rom, filelist[selection].length);
			break;
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
* VMCPULoadROM
*
* VM version of GBA CPULoadROM
****************************************************************************/

int VMCPULoadROM(int method)
{
	int res;
	char msg[512];
	char filepath[MAXPATHLEN];

	/** Fix VM **/
	VMClose();
	VMInit();
	VMAllocGBA();

	GBAROMSize = 0;

	switch (method)
	{
		case METHOD_SD:
		case METHOD_USB:
		break;

		case METHOD_DVD:
			VMClose();
			return 0; // not implemented
		break;

		case METHOD_SMB:
			VMClose();
			return 0; // not implemented
		break;
	}

	/* Check filename length */
	if ((strlen(currentdir)+1+strlen(filelist[selection].filename)) < MAXPATHLEN)
		sprintf(filepath, "%s/%s",currentdir,filelist[selection].filename);
	else
	{
		WaitPrompt((char*) "Maximum filepath length reached!");
		return -1;
	}

	romfile = fopen(filepath, "rb");
	if ( romfile == NULL )
	{
		WaitPrompt((char*) "Error opening file!");
		VMClose();
		return 0;
	}

	res = fread(rom, 1, (1 << VMSHIFTBITS), romfile);
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
	int res;
	//tb_t start,end;
	char msg[512];

	//mftb(&start);

	res = fseek( romfile, pageid << VMSHIFTBITS, SEEK_SET );
	if (res) // fseek returns non-zero on a failure
	{
		sprintf(msg, "Seek error! - Offset %d / %08x %d\n", pageid, pageid << VMSHIFTBITS, res);
		WaitPrompt(msg);
		VMClose();
		return;
	}

	VMAllocate( pageid );

	res = fread( vmpage[pageid].pageptr, 1, 1 << VMSHIFTBITS, romfile );
	if ( res != ( 1 << VMSHIFTBITS ) )
	{
		sprintf(msg, "Error reading! %d bytes only\n", res);
		WaitPrompt(msg);
		VMClose();
		return;
	}

	//mftb(&end);

	//loadtimeradjust += tb_diff_msec(&end, &start);
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

	if ( address >= (u32)GBAROMSize )
	{
		badaddress = ( ( ( address >> 1 ) & 0xffff ) << 16 ) | ( ( ( address + 2 ) >> 1 ) & 0xffff );
		return badaddress;
	}

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
}

/****************************************************************************
 * VMRead16
 *
 * Return a 16bit value
 ****************************************************************************/
u16 VMRead16( u32 address )
{
	int pageid;

	if ( address >= (u32)GBAROMSize )
	{
		return ( address >> 1 ) & 0xffff;
	}

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
}

/****************************************************************************
 * VMRead8
 *
 * Return 8bit value
 ****************************************************************************/
u8 VMRead8( u32 address )
{
	int pageid;

	if ( address >= (u32)GBAROMSize )
	{
		return ( address >> 1 ) & 0xff;
	}

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
}

#endif

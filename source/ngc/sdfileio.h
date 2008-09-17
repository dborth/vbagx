/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * sdfileio.h
 *
 * Generic File I/O for VisualBoyAdvance
 * Currently only supports SD/USB
****************************************************************************/
#ifndef __SDFILEIO__
#define __SDFILEIO__


#define MAXDIRENTRIES 1000
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/dir.h>

extern "C"
  {

    /* Required Functions */
    FILE* gen_fopen( const char *filename, const char *mode );
    int gen_fwrite( const void *buffer, int len, int block, FILE* f );
    int gen_fread( void *buffer, int len, int block, FILE* f );
    void gen_fclose( FILE* f );
    int gen_fseek(FILE* f, int where, int whence);
    int gen_fgetc( FILE* f );
    int SDInit( void );
    int gen_getdir( char *thisdir );
    extern char direntries[MAXDIRENTRIES][255];
  }

#endif


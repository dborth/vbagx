/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * sdfileio.c
 *
 * Generic File I/O for VisualBoyAdvance
 * Currently only supports SD/USB
****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fat.h>
#include <sys/dir.h>

#define MAXDIRENTRIES 1000
char direntries[MAXDIRENTRIES][255];

/**
 * SDInit
 */
void SDInit( void )
{
	fatInitDefault();
}

/**
 * SD Card f_open
 */
FILE* gen_fopen( const char *filename, const char *mode )
{
	return fopen( filename, mode );
}

/**
 * SD Card f_write
 */
int gen_fwrite( const void *buffer, int len, int block, FILE* f )
{
	return fwrite(buffer, len, block, f);
}

/**
 * SD Card f_read
 */
int gen_fread( void *buffer, int len, int block, FILE* f )
{

	return fread(buffer, len, block, f);
}

/**
 * SD Card fclose
 */
void gen_fclose( FILE* f )
{
	fclose(f);
}

/**
 * SD Card fseek
 *
 * NB: Only supports SEEK_SET
 */
int gen_fseek(FILE* f, int where, int whence)
{
	fseek(f, where, whence);
	return 1;
}

/**
 * Simple fgetc
 */
int gen_fgetc( FILE* f )
{
	return fgetc(f);
}

static struct stat _fstat;
char filename[1024];
int fcount = 0;

/**
 * Get directory listing
 */
int gen_getdir( char *thisdir )
{
  memset(&direntries[0],0,MAXDIRENTRIES*255);

  DIR_ITER* dp = diropen( thisdir );

  if ( dp )
    {
      while ( dirnext(dp, filename, &_fstat) == 0 )
        {

          // Skip any sub directories
          if ( !(_fstat.st_mode & S_IFDIR) )
            {
              memcpy(&direntries[fcount],&filename,strlen(filename));
              fcount++;
            }
        }
        dirclose(dp);
    }
    else
    	return 0;


  return fcount;

}


/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * gcunzip.h
 *
 * File unzip routines
 ***************************************************************************/
#ifndef _GCUNZIP_H_
#define _GCUNZIP_H_

int IsZipFile (char *buffer);
char * GetFirstZipFilename();
size_t UnZipBuffer (unsigned char *outbuffer, size_t buffersize);
int SzParse(char * filepath);
size_t SzExtractFile(int i, unsigned char *buffer);
void SzClose();

#endif

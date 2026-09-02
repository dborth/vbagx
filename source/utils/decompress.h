/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * decompress.h
 *
 * File unzip routines
 ***************************************************************************/
#ifndef _decompress_H_
#define _decompress_H_

int IsZipFile (char *buffer);
char * GetFirstZipFilename();
size_t UnZipBuffer (unsigned char *outbuffer, size_t buffersize);
int SzParse(char * filepath);
size_t SzExtractFile(int i, unsigned char *buffer);
void SzClose();

#endif

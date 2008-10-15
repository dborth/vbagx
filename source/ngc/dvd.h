/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * dvd.h
 *
 * DVD I/O functions
 ***************************************************************************/

#ifndef _NGCDVD_
#define _NGCDVD_

int getpvd ();
int ParseDVDdirectory ();
int LoadDVDFile (unsigned char *buffer, int length);
bool TestDVD();
int dvd_read (void *dst, unsigned int len, u64 offset);
int dvd_safe_read (void *dst, unsigned int len, u64 offset);
bool SwitchDVDFolder(char dir[]);
void SetDVDDriveType();
#ifdef HW_DOL
void dvd_motor_off ();
#endif

extern u64 dvddir;
extern int dvddirlength;

#endif

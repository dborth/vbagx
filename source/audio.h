/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * audio.h
 *
 * Head and tail audio mixer
 ***************************************************************************/

#ifndef __AUDIOMIXER__
#define __AUDIOMIXER__

#include "vba/common/SoundDriver.h"

void InitialiseSound();
void StopAudio();
void SwitchAudioMode(int mode);
int AudioGetUnplayed();

class SoundWii: public SoundDriver
{
public:
	SoundWii();
	virtual double getDynamicRate();
	virtual bool canWrite();
	virtual u16* getWriteBuffer();
	virtual void commitWrite();
};

#endif

/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2023
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
void SetAudioRate(int type);
void SwitchAudioMode(int mode);
void ShutdownAudio();

class SoundWii: public SoundDriver
{
public:
	SoundWii();
	virtual ~SoundWii();

	virtual bool init(long sampleRate);
	virtual void pause();
	virtual void reset();
	virtual void resume();
	virtual void write(u16 * finalWave, int length);
};

#endif

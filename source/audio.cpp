/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2023
 *
 * audio.cpp
 *
 * Head and tail audio mixer
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <asndlib.h>

#include "audio.h"

extern int ConfigRequested;

/** Locals **/
static int head = 0;
static int tail = 0;
static int gameType = 0;

#define MIXBUFFSIZE 0x10000
static u8 mixerdata[MIXBUFFSIZE];
#define MIXERMASK ((MIXBUFFSIZE >> 2) - 1)
#define SWAP(x) ((x>>16)|(x<<16)) // for reversing stereo channels

static u8 soundbuffer[2][3840] ATTRIBUTE_ALIGN(32);
static int whichab = 0;
static int IsPlaying = 0;

/****************************************************************************
 * MIXER_GetSamples
 ***************************************************************************/
static int MIXER_GetSamples(u8 *dstbuffer, int maxlen)
{
	u32 *src = (u32 *)mixerdata;
	u32 *dst = (u32 *)dstbuffer;
	u32 intlen = maxlen >> 2;

	memset(dstbuffer, 0, maxlen);

	while( ( head != tail ) && intlen )
	{
		*dst++ = src[tail++];
		tail &= MIXERMASK;
		intlen--;
	}

	return 3200;
}

/****************************************************************************
 * AudioPlayer
 ***************************************************************************/

static void AudioPlayer()
{
	if (!ConfigRequested)
	{
		whichab ^= 1;
		int len = MIXER_GetSamples(soundbuffer[whichab], 3200);
		DCFlushRange(soundbuffer[whichab],len);
		AUDIO_InitDMA((u32)soundbuffer[whichab],len);
		IsPlaying = 1;
	}
	else
		IsPlaying = 0;
}

/****************************************************************************
 * StopAudio
 ***************************************************************************/

void StopAudio()
{
	AUDIO_StopDMA();
	IsPlaying = 0;
}

/****************************************************************************
 * SetAudioRate
 ***************************************************************************/

void SetAudioRate(int type)
{
	gameType = type;
}

/****************************************************************************
 * SwitchAudioMode
 *
 * Switches between menu sound and emulator sound
 ***************************************************************************/
void
SwitchAudioMode(int mode)
{
	if(mode == 0) // emulator
	{
		#ifndef NO_SOUND
		ASND_Pause(1);
		ASND_End();
		AUDIO_StopDMA();
		AUDIO_RegisterDMACallback(NULL);
		DSP_Halt();
		AUDIO_RegisterDMACallback(AudioPlayer);
		#endif
		memset(soundbuffer[0],0,3840);
		memset(soundbuffer[1],0,3840);
		DCFlushRange(soundbuffer[0],3840);
		DCFlushRange(soundbuffer[1],3840);
		AUDIO_InitDMA((u32)soundbuffer[whichab],3200);
		AUDIO_StartDMA();
	}
	else // menu
	{
		IsPlaying = 0;
		#ifndef NO_SOUND
		DSP_Unhalt();
		ASND_Init();
		ASND_Pause(0);
		#else
		AUDIO_StopDMA();
		#endif
	}
}

/****************************************************************************
 * InitialiseSound
 ***************************************************************************/

void InitialiseSound()
{
	#ifdef NO_SOUND
	AUDIO_Init (NULL);
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AUDIO_RegisterDMACallback(AudioPlayer);
	#else
	ASND_Init();
	#endif
}

/****************************************************************************
 * ShutdownAudio
 *
 * Shuts down audio subsystem. Useful to avoid unpleasant sounds if a
 * crash occurs during shutdown.
 ***************************************************************************/
void ShutdownAudio()
{
	AUDIO_StopDMA();
}

/****************************************************************************
 * SoundDriver
 ***************************************************************************/

SoundWii::SoundWii()
{
	memset(soundbuffer, 0, 3840*2);
	memset(mixerdata, 0, MIXBUFFSIZE);
}

/****************************************************************************
* SoundWii::write
*
* Upsample from 11025 to 48000
* 11025 == 15052
* 22050 == 30106
* 44100 == 60211
*
* Audio officianados should look away now !
****************************************************************************/

void SoundWii::write(u16 * finalWave, int length)
{
	u32 *src = (u32 *)finalWave;
	u32 *dst = (u32 *)mixerdata;
	u32 intlen = (3200 >> 2);
	u32 fixofs = 0;
	u32 fixinc = 60211; // length = 2940 - GB

	if (gameType == 2) // length = 1468 - GBA
		fixinc = 30065;

	do
	{
		// Do simple linear interpolate, and swap channels from L-R to R-L
		dst[head++] = SWAP(src[fixofs >> 16]);
		head &= MIXERMASK;
		fixofs += fixinc;
	}
	while( --intlen );

	// Restart Sound Processing if stopped
	if (IsPlaying == 0)
	{
		ConfigRequested = 0;
		AudioPlayer();
	}
}

bool SoundWii::init(long sampleRate)
{
	return true;
}

SoundWii::~SoundWii()
{
}

void SoundWii::pause()
{
}

void SoundWii::resume()
{
}

void SoundWii::reset()
{
}

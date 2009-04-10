/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * audio.cpp
 *
 * Head and tail audio mixer
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	if ( !ConfigRequested )
	{
		int len = MIXER_GetSamples(soundbuffer[whichab], 3200);
		DCFlushRange(soundbuffer[whichab],len);
		AUDIO_InitDMA((u32)soundbuffer[whichab],len);
		AUDIO_StartDMA();
		whichab ^= 1;
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
 * InitialiseSound
 ***************************************************************************/

void InitialiseSound()
{
	AUDIO_Init(NULL); // Start audio subsystem
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AUDIO_RegisterDMACallback(AudioPlayer);
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
	u32 fixinc;

	if (gameType == 2) // length = 1468 - GBA
		fixinc = 30065;
	else // length = 2940 - GB
		fixinc = 60211;

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

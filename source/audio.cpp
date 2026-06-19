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
// head is written by the emulator thread (write) and read by the DMA
// callback (interrupt context); tail is the reverse. They MUST be volatile
// so the compiler does not cache them in registers across the thread/ISR
// boundary, otherwise the consumer never observes the producer's updates.
static volatile int head = 0;
static volatile int tail = 0;
static int gameType = 0;

#define MIXBUFFSIZE 0x10000
// Accessed as u32 below, and (indirectly) feeds DMA, so require 32-byte
// alignment. Without this the u32 casts rely on undefined alignment.
static u8 mixerdata[MIXBUFFSIZE] ATTRIBUTE_ALIGN(32);
#define MIXERMASK ((MIXBUFFSIZE >> 2) - 1)
#define SWAP(x) (((x)>>16) | ((x)<<16)) // for reversing stereo channels

// One DMA frame is 3200 bytes (800 stereo 16-bit frames). The hardware
// buffer is sized to 3840 with 32-byte-aligned length headroom.
#define DMA_BYTES 3200

static u8 soundbuffer[2][3840] ATTRIBUTE_ALIGN(32);
static int whichab = 0;
// Read by the emulator thread (write) and written by the DMA callback.
static volatile int IsPlaying = 0;

/****************************************************************************
 * MIXER_GetSamples
 *
 * Drains up to maxlen bytes from the ring buffer into dstbuffer. Any space
 * not filled (a buffer underrun) is left as silence by the initial memset.
 * Returns the number of bytes the caller should hand to the DMA engine,
 * which is always a fixed-size, 32-byte-aligned frame.
 ***************************************************************************/
static int MIXER_GetSamples(u8 *dstbuffer, int maxlen)
{
	u32 *src = (u32 *)mixerdata;
	u32 *dst = (u32 *)dstbuffer;
	u32 intlen = maxlen >> 2;

	memset(dstbuffer, 0, maxlen);

	// Snapshot the producer index once. head is volatile and updated from
	// the emulator thread; re-reading it in the loop condition could let the
	// consumer chase a moving target and over-read.
	int producer = head;

	while ( ( producer != tail ) && intlen )
	{
		*dst++ = src[tail++];
		tail &= MIXERMASK;
		intlen--;
	}

	return maxlen;
}

/****************************************************************************
 * AudioPlayer
 ***************************************************************************/

static void AudioPlayer()
{
	if (!ConfigRequested)
	{
		whichab ^= 1;
		int len = MIXER_GetSamples(soundbuffer[whichab], DMA_BYTES);
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
	u32 intlen = (DMA_BYTES >> 2);
	u32 fixofs = 0;
	u32 fixinc = 60211; // length = 2940 - GB

	if (gameType == 2) // length = 1468 - GBA
		fixinc = 30065;

	// length is given in u16 samples; the source is read as u32 (one packed
	// stereo frame), so clamp the highest index we may read. Previously the
	// length argument was ignored, allowing reads past finalWave.
	u32 maxSrcIndex = (length > 1) ? (u32)((length >> 1) - 1) : 0;

	// Work on a local copy of the volatile producer index and publish it once
	// at the end. This avoids a memory round-trip on every loop iteration and
	// lets us bounds-check against the consumer to prevent overrunning audio
	// that has not been played yet.
	int localHead = head;
	int consumer = tail;

	do
	{
		u32 srcIndex = fixofs >> 16;
		if (srcIndex > maxSrcIndex)
			srcIndex = maxSrcIndex;

		int next = (localHead + 1) & MIXERMASK;
		if (next == consumer)
			break; // ring buffer full: drop rather than clobber unplayed data

		// Do simple linear interpolate, and swap channels from L-R to R-L
		dst[localHead] = SWAP(src[srcIndex]);
		localHead = next;
		fixofs += fixinc;
	}
	while( --intlen );

	head = localHead; // publish to the DMA callback

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

/****************************************************************************
* VisualBoyAdvance
*
* Head and tail audio mixer
****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Locals **/
static int head = 0;
static int tail = 0;

#define MIXBUFFSIZE 0x10000
static u8 mixerdata[MIXBUFFSIZE];
#define MIXERMASK ((MIXBUFFSIZE >> 2) - 1)

static u8 soundbuffer[3200] ATTRIBUTE_ALIGN(32);

/****************************************************************************
* MIXER_AddSamples
*
* Upsample from 11025 to 48000
* 11025 == 15052
* 22050 == 30106
* 44100 == 60211
*
* Audio officianados should look away now !
****************************************************************************/
void MIXER_AddSamples( u8 *sampledata, int len )
{
	u32 *src = (u32 *)sampledata;
	u32 *dst = (u32 *)mixerdata;
	u32 intlen = (3200 >> 2);
	u32 fixofs = 0;
	u32 fixinc;

	if ( !len )
		fixinc = 30106;
	else
		fixinc = 60211;

	do
	{
		// Do simple linear interpolate
		dst[head++] = src[fixofs >> 16];
		head &= MIXERMASK;
		fixofs += fixinc;
	}
	while( --intlen );
}

/****************************************************************************
* MIXER_GetSamples
****************************************************************************/
int MIXER_GetSamples( u8 *dstbuffer, int maxlen )
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

static void AudioPlayer()
{
	AUDIO_StopDMA();
	MIXER_GetSamples(soundbuffer, 3200);
	DCFlushRange(soundbuffer,3200);
	AUDIO_InitDMA((u32)soundbuffer,3200);
	AUDIO_StartDMA();
}

void InitialiseSound()
{
	AUDIO_Init(NULL); // Start audio subsystem
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AUDIO_RegisterDMACallback(AudioPlayer);
	memset(soundbuffer, 0, 3200);
}

void StopAudio()
{
	AUDIO_StopDMA();
}

void StartAudio()
{
	AUDIO_StartDMA();
}

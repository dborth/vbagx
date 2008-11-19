/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * audio.h
 *
 * Head and tail audio mixer
 ***************************************************************************/

#ifndef __AUDIOMIXER__
#define __AUDIOMIXER__

void MIXER_AddSamples( u8 *sampledata, int len );
void StopAudio();
void ResetAudio();
void InitialiseSound();

#endif

/****************************************************************************
* VisualBoyAdvance
*
* Head and tail audio mixer
****************************************************************************/
#ifndef __AUDIOMIXER__
#define __AUDIOMIXER__

void MIXER_AddSamples( u8 *sampledata, int len );
int MIXER_GetSamples( u8 *dstbuffer, int maxlen );
void StopAudio();
void StartAudio();
void InitialiseSound();

#endif

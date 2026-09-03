/****************************************************************************
 * Visual Boy Advance GX - drivers/ogc
 * Daryl Borth 2008-2026
 * OgcEmulatorAudio.h
 *
 * Direct-queued audio driver with dynamic rate control. Feeds the VBA
 * core's SoundDriver interface and drives the hardware DMA ring buffer.
 ***************************************************************************/
#pragma once

#include "vba/common/SoundDriver.h"

void AudioDMACallback();
void AudioReset();
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

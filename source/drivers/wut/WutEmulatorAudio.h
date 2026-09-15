/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * WutEmulatorAudio.h
 *
 * EmulatorAudioDriver implementation for Wii U
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <sndcore2/voice.h>
#include "../EmulatorAudioDriver.h"

class WutEmulatorAudio : public EmulatorAudioDriver
{
	public:
		WutEmulatorAudio();
		~WutEmulatorAudio() override;

		void init() override {}
		void resetAudio() override {}
		int getUnplayed() override { return 0; }

		double getDynamicRate() override { return 0.0; }
		bool canWrite() override { return false; }
		u16* getWriteBuffer() override { return nullptr; }
		void commitWrite() override {}

	private:

};

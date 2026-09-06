/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * EmulatorAudioDriver.h
 ***************************************************************************/
#pragma once

#include "../vba/common/Types.h"

class EmulatorAudioDriver
{
	public:
		virtual ~EmulatorAudioDriver() = default;

		virtual void init() = 0;

		//! Clears buffered/queued audio state and dynamic-rate hysteresis.
		//! Called when loading a new game.
		virtual void resetAudio() = 0;

		//! Number of DMA buffers currently queued but not yet played, or -1
		//! before DMA has primed. Used by vbasupport.cpp's frame-pacing
		//! skip-pressure model.
		virtual int getUnplayed() = 0;

		// --- Sound-output contract the VBA core mixes samples through ---
		// (formerly the standalone vba::SoundDriver interface)

		//! Playback speed multiplier the core should target this frame, to
		//! keep the queue neither starved nor overfull.
		virtual double getDynamicRate() = 0;

		//! Is there room in the ring for one more buffer right now?
		virtual bool canWrite() = 0;

		//! The next ring slot the core should mix samples into.
		virtual u16* getWriteBuffer() = 0;

		//! Publishes the buffer returned by getWriteBuffer() for playback.
		virtual void commitWrite() = 0;
};

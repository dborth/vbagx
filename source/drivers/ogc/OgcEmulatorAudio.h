/****************************************************************************
 * Visual Boy Advance GX - drivers/ogc
 * Daryl Borth 2008-2026
 * OgcEmulatorAudio.h
 *
 * Direct-queued audio driver with dynamic rate control. Feeds the VBA
 * core's sound output and drives the hardware DMA ring buffer.
 ***************************************************************************/
#pragma once

#include <gctypes.h>
#include "../EmulatorAudioDriver.h"

// Hardware DMA callback trampoline, registered with AUDIO_RegisterDMACallback()
// which requires a bare C function pointer.
void AudioDMACallback();

class OgcEmulatorAudio : public EmulatorAudioDriver
{
	public:
		OgcEmulatorAudio();
		~OgcEmulatorAudio() override;

		void init() override {}
		void resetAudio() override;
		int getUnplayed() override;

		double getDynamicRate() override;
		bool canWrite() override;
		u16* getWriteBuffer() override;
		void commitWrite() override;

		// Called only via the AudioDMACallback trampoline above.
		void dmaCallback();

	private:
		// One DMA frame is 3200 bytes (800 stereo 16-bit frames).
		static constexpr int DMA_BYTES = 3200;

		// BUFFERCOUNT must be a power of two so the ring index can advance
		// with a cheap bitwise mask (see nextIndex) instead of an integer modulo.
		static constexpr int BUFFERCOUNT = 16;
		static constexpr int MAX_QUEUED_BUFFERS = 12; // Leave a 4-buffer safety zone to prevent input lag

		// Number of stereo frames over which we ramp to/from zero when the
		// ring runs genuinely dry. Long enough to remove the audible click
		// of a hard jump to silence, short enough (~2ms) to add no
		// perceptible latency.
		static constexpr int FADE_FRAMES = 96;

		// Discrete state of the dynamic-rate controller (hysteresis pitch
		// bending). Only touched outside interrupt context (getDynamicRate),
		// so it needs no synchronization.
		enum RateState {
			RATE_STATE_NEUTRAL,
			RATE_STATE_DRAINING,  // running slow to shrink an over-full queue
			RATE_STATE_FILLING,   // running fast to grow an under-full queue
		};

		static int nextIndex(int current) { return (current + 1) & (BUFFERCOUNT - 1); }
		int getUnplayedInternal() const { return (nextab - playab + BUFFERCOUNT) & (BUFFERCOUNT - 1); }

		void buildFadeOutBuffer();
		void applyFadeIn(u8* buf);

		u8 soundbuffer[BUFFERCOUNT][DMA_BYTES] __attribute__((aligned(32)));
		u8 silence[DMA_BYTES] __attribute__((aligned(32)));
		u8 fadeBuffer[DMA_BYTES] __attribute__((aligned(32)));

		// Volatile indices crossing the emulator-thread/ISR boundary (MUST bypass registers)
		volatile int playab;
		volatile int nextab;

		// Only touched outside interrupt context (no volatile needed)
		bool dma_started;
		RateState rateState;

		// Declick state -- tracks the tail of the last real audio actually
		// queued, so a starvation event can ramp down from where the
		// waveform really was instead of snapping to zero, and ramp back in
		// the same way on recovery.
		s16 lastL;
		s16 lastR;
		bool wasStarved;
};

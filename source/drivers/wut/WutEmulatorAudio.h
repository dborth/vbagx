/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * WutEmulatorAudio.h
 *
 * AX-backed EmulatorAudioDriver implementation for Wii U. Feeds the VBA
 * core's sound output through a continuously-looping AX ring buffer with
 * dynamic rate control. Adapted to AX's frame-callback model.
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

		// Acquires the two AX voices and configures their fixed (never
		// touched again) format/loop/mix/SRC-bypass settings. Must be
		// called after AXInitWithParams() -- ie. from WutAudioDriver::init().
		void init() override;
		void resetAudio() override;
		int getUnplayed() override;

		double getDynamicRate() override;
		bool canWrite() override;
		u16* getWriteBuffer() override;
		void commitWrite() override;

		//! Arms playback once commitWrite() has pre-rolled enough frames.
		//! Not part of the EmulatorAudioDriver interface -- called by
		//! commitWrite() itself and by WutAudioDriver::startEmulatorAudio().
		void start();

		//! Hard-stops both voices (leaving to the menu, or shutdown).
		void stop();

		//! Frees the two AX voices. Must be called before AXQuit().
		void shutdown();

		//! AX frame-callback hook (~3ms cadence)
		void frameTick();

	private:
		// One Sound.cpp flush_samples() chunk: (soundSampleRate/60) frames.
		// soundSampleRate is fixed at 48000 (see vba/gba/Sound.cpp)
		static constexpr int COMMIT_FRAMES = 800;

		// Ring capacity / thresholds, in frames
		static constexpr int RING_FRAMES        = 16 * COMMIT_FRAMES; // 12800 (~267ms)
		static constexpr int MAX_QUEUED_FRAMES  = 12 * COMMIT_FRAMES; // 4-buffer safety zone
		static constexpr int HIGH_WATER_FRAMES  = 8  * COMMIT_FRAMES;
		static constexpr int HIGH_RELEASE_FRAMES= 6  * COMMIT_FRAMES;
		static constexpr int LOW_RELEASE_FRAMES = 6  * COMMIT_FRAMES;
		static constexpr int LOW_WATER_FRAMES   = 4  * COMMIT_FRAMES;
		static constexpr int CRITICAL_FRAMES    = 1  * COMMIT_FRAMES;
		static constexpr int HIGH_CRITICAL_FRAMES = 11 * COMMIT_FRAMES;
		static constexpr int START_LEVEL_FRAMES = 6  * COMMIT_FRAMES;

		static constexpr double RATE_SLOW_DOWN = 1.005;
		static constexpr double RATE_SPEED_UP = 0.995;
		static constexpr double RATE_EMERGENCY_SLOW_DOWN = 1.015;
		static constexpr double RATE_EMERGENCY_SPEED_UP = 0.985;
		static constexpr double RATE_NEUTRAL = 1.0;

		// Native AX hardware volume-envelope ramp, used to duck/unduck on
		// a transient starvation instead of an abrupt AXSetVoiceState
		// stop (which pops). Values are a starting point, not verified
		// against real hardware timing yet -- worth confirming by ear.
		static constexpr uint16_t AX_MAX_VOLUME = 0x8000;
		static constexpr int DUCK_RAMP_SAMPLES = 64; // ~1.3ms at 48kHz

		// AXSetVoiceDeviceMix output channel counts (TV/DRC)
		static constexpr int AX_TV_CHANNELS = 6;
		static constexpr int AX_DRC_CHANNELS = 4;

		enum RateState { RATE_STATE_NEUTRAL, RATE_STATE_DRAINING, RATE_STATE_FILLING };

		void configureVoice(AXVoice* v, int16_t* ringBuf, bool isLeft);
		void writeFrames(const int16_t* interleavedSrc, uint32_t frames);
		uint32_t queryUnplayedFrames();
		void rampVolume(AXVoice* v, uint16_t startVolume, uint16_t targetVolume);

		AXVoice* voiceL = nullptr;
		AXVoice* voiceR = nullptr;

		alignas(32) int16_t ringL[RING_FRAMES];
		alignas(32) int16_t ringR[RING_FRAMES];

		// Sound.cpp writes one interleaved 800-frame chunk here per
		// commitWrite() call; commitWrite() deinterleaves it into ringL/R.
		alignas(32) int16_t stagingInterleaved[COMMIT_FRAMES * 2];

		// Owned exclusively by commitWrite()/the main thread.
		uint32_t writePos = 0;
		volatile bool started = false;

		// Owned exclusively by frameTick()/the AX callback context.
		bool ducked = false;

		RateState rateState = RATE_STATE_NEUTRAL;
};

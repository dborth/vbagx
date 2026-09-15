/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * WutEmulatorAudio.cpp
 ***************************************************************************/
#include <coreinit/cache.h>
#include <sndcore2/core.h>
#include <string.h>

#include "WutEmulatorAudio.h"
#include "../Logger.h"
#include "../../vbagx.h"
#include "../../vba/gba/Globals.h" // extern bool turboMode;

WutEmulatorAudio::WutEmulatorAudio()
{
	memset(ringL, 0, sizeof(ringL));
	memset(ringR, 0, sizeof(ringR));
	memset(stagingInterleaved, 0, sizeof(stagingInterleaved));
}

WutEmulatorAudio::~WutEmulatorAudio()
{
	if (voiceL || voiceR)
		shutdown();
}

/****************************************************************************
 * configureVoice
 *
 * Sets everything about a voice that never changes again after init():
 * format, hard L/R pan via device mix.
 ***************************************************************************/
void WutEmulatorAudio::configureVoice(AXVoice* v, int16_t* ringBuf, bool isLeft)
{
	AXVoiceBegin(v);
	AXSetVoiceType(v, 0);

	AXVoiceDeviceMixData mix;
	memset(&mix, 0, sizeof(mix));
	mix.bus[0].volume = isLeft ? AX_MAX_VOLUME : 0;
	mix.bus[1].volume = isLeft ? 0 : AX_MAX_VOLUME;
	AXSetVoiceDeviceMix(v, AX_DEVICE_TYPE_TV, 0, &mix);
	AXSetVoiceDeviceMix(v, AX_DEVICE_TYPE_DRC, 0, &mix);

	AXVoiceSrc src;
	memset(&src, 0, sizeof(src));
	src.ratio = 0x00010000; // 1.0 in 16.16 -- see rationale above.
	AXSetVoiceSrc(v, &src);
	AXSetVoiceSrcType(v, AX_VOICE_SRC_TYPE_NONE);

	AXVoiceOffsets offs;
	memset(&offs, 0, sizeof(offs));
	offs.dataType = AX_VOICE_FORMAT_LPCM16;
	offs.loopingEnabled = AX_VOICE_LOOP_ENABLED;
	offs.loopOffset = 0;
	offs.endOffset = RING_FRAMES - 1;
	offs.currentOffset = 0;
	offs.data = ringBuf;
	AXSetVoiceOffsets(v, &offs);

	AXVoiceVeData ve;
	ve.volume = AX_MAX_VOLUME;
	ve.delta = 0;
	AXSetVoiceVe(v, &ve);

	AXVoiceEnd(v);
}

void WutEmulatorAudio::init()
{
	voiceL = AXAcquireVoice(31, nullptr, nullptr);
	voiceR = AXAcquireVoice(31, nullptr, nullptr);

	DCFlushRange(ringL, sizeof(ringL));
	DCFlushRange(ringR, sizeof(ringR));

	configureVoice(voiceL, ringL, true);
	configureVoice(voiceR, ringR, false);

	resetAudio();
}

void WutEmulatorAudio::shutdown()
{
	stop();
	if (voiceL) { AXFreeVoice(voiceL); voiceL = nullptr; }
	if (voiceR) { AXFreeVoice(voiceR); voiceR = nullptr; }
}

/****************************************************************************
 * resetAudio
 *
 * Cheap reset
 ***************************************************************************/
void WutEmulatorAudio::resetAudio()
{
	writePos = 0;
	started = false;
	ducked = false;
	rateState = RATE_STATE_NEUTRAL;

	if (voiceL) { AXSetVoiceState(voiceL, AX_VOICE_STATE_STOPPED); AXSetVoiceCurrentOffset(voiceL, 0); }
	if (voiceR) { AXSetVoiceState(voiceR, AX_VOICE_STATE_STOPPED); AXSetVoiceCurrentOffset(voiceR, 0); }
}

void WutEmulatorAudio::stop()
{
	if (voiceL) AXSetVoiceState(voiceL, AX_VOICE_STATE_STOPPED);
	if (voiceR) AXSetVoiceState(voiceR, AX_VOICE_STATE_STOPPED);
	started = false;
	ducked = false;
}

void WutEmulatorAudio::start()
{
	if (started || !voiceL || !voiceR)
		return;

	// Pre-roll already sitting in the ring (written by commitWrite() before
	// this was called) starts at offset 0; arm both voices back-to-back.
	AXSetVoiceCurrentOffset(voiceL, 0);
	AXSetVoiceCurrentOffset(voiceR, 0);
	AXSetVoiceState(voiceL, AX_VOICE_STATE_PLAYING);
	AXSetVoiceState(voiceR, AX_VOICE_STATE_PLAYING);
	started = true;
	ducked = false;
}

/****************************************************************************
 * queryUnplayedFrames
 *
 * voiceL is the timing master: both voices share the same ring length,
 * SRC-bypassed 1:1 ratio, and were started together, so voiceR's position
 * is never independently queried -- there is nothing for it to drift
 * against.
 ***************************************************************************/
uint32_t WutEmulatorAudio::queryUnplayedFrames()
{
	if (!voiceL) return 0;
	uint32_t currentOffset = AXGetVoiceCurrentOffsetEx(voiceL, ringL);
	return (writePos - currentOffset + RING_FRAMES) % RING_FRAMES;
}

int WutEmulatorAudio::getUnplayed()
{
	if (!started) return -1;
	return (int)(queryUnplayedFrames() / COMMIT_FRAMES);
}

bool WutEmulatorAudio::canWrite()
{
	if (appRequest == AppRequest::MENU)
	{
		resetAudio();
		return false;
	}
	return queryUnplayedFrames() < MAX_QUEUED_FRAMES;
}

double WutEmulatorAudio::getDynamicRate()
{
	if (turboMode)
	{
		rateState = RATE_STATE_NEUTRAL;
		return RATE_NEUTRAL;
	}

	uint32_t unplayed = queryUnplayedFrames();

	if (rateState == RATE_STATE_DRAINING && unplayed <= HIGH_RELEASE_FRAMES)
		rateState = RATE_STATE_NEUTRAL;
	else if (rateState == RATE_STATE_FILLING && unplayed >= LOW_RELEASE_FRAMES)
		rateState = RATE_STATE_NEUTRAL;

	if (unplayed > HIGH_WATER_FRAMES)
		rateState = RATE_STATE_DRAINING;
	else if (unplayed < LOW_WATER_FRAMES)
		rateState = RATE_STATE_FILLING;

	if (rateState == RATE_STATE_DRAINING)
		return (unplayed >= HIGH_CRITICAL_FRAMES) ? RATE_EMERGENCY_SLOW_DOWN : RATE_SLOW_DOWN;
	else if (rateState == RATE_STATE_FILLING)
		return (unplayed <= CRITICAL_FRAMES) ? RATE_EMERGENCY_SPEED_UP : RATE_SPEED_UP;

	return RATE_NEUTRAL;
}

u16* WutEmulatorAudio::getWriteBuffer()
{
	return (u16*)stagingInterleaved;
}

/****************************************************************************
 * writeFrames
 *
 * Deinterleaves `frames` stereo s16 frames from `interleavedSrc` into the
 * two planar rings at writePos, handling ring wraparound, and flushes the
 * touched cache lines.
 ***************************************************************************/
void WutEmulatorAudio::writeFrames(const int16_t* interleavedSrc, uint32_t frames)
{
	uint32_t firstRun = (frames < (uint32_t)(RING_FRAMES - writePos)) ? frames : (uint32_t)(RING_FRAMES - writePos);
	uint32_t secondRun = frames - firstRun;

	for (uint32_t i = 0; i < firstRun; i++)
	{
		ringL[writePos + i] = interleavedSrc[i * 2];
		ringR[writePos + i] = interleavedSrc[i * 2 + 1];
	}
	DCFlushRange(&ringL[writePos], firstRun * sizeof(int16_t));
	DCFlushRange(&ringR[writePos], firstRun * sizeof(int16_t));

	for (uint32_t i = 0; i < secondRun; i++)
	{
		ringL[i] = interleavedSrc[(firstRun + i) * 2];
		ringR[i] = interleavedSrc[(firstRun + i) * 2 + 1];
	}
	if (secondRun > 0)
	{
		DCFlushRange(ringL, secondRun * sizeof(int16_t));
		DCFlushRange(ringR, secondRun * sizeof(int16_t));
	}

	writePos = (writePos + frames) % RING_FRAMES;
}

void WutEmulatorAudio::commitWrite()
{
	writeFrames(stagingInterleaved, COMMIT_FRAMES);

	if (!started && queryUnplayedFrames() >= START_LEVEL_FRAMES)
		start();

	// Fresh audio has arrived -- if the frame callback ducked us for a
	// transient underrun, ramp back up now that there's real signal again.
	if (ducked && queryUnplayedFrames() >= COMMIT_FRAMES)
	{
		rampVolume(voiceL, AX_MAX_VOLUME);
		rampVolume(voiceR, AX_MAX_VOLUME);
		ducked = false;
	}
}

void WutEmulatorAudio::rampVolume(AXVoice* v, int16_t targetVolume)
{
	if (!v) return;
	AXVoiceVeData ve;
	ve.volume = (uint16_t)v->volume;
	ve.delta = (int16_t)(((int32_t)targetVolume - (int32_t)ve.volume) / DUCK_RAMP_SAMPLES);
	AXSetVoiceVe(v, &ve);
}

/****************************************************************************
 * frameTick
 *
 * Called from WutAudioDriver's single AX frame callback (~3ms) while
 * emulator audio is the active mode. Deliberately read-only with respect
 * to writePos/ring content.
 ***************************************************************************/
void WutEmulatorAudio::frameTick()
{
	if (!started || ducked)
		return;

	uint32_t unplayed = queryUnplayedFrames();
	uint32_t tickFrames = AXGetInputSamplesPerFrame();

	if (unplayed < tickFrames)
	{
		// Caught up to our own write edge -- ramp to silence via AX's
		// native volume envelope rather than an abrupt state-stop (which
		// pops), and let commitWrite() ramp back up once fresh samples
		// build back up past a chunk's worth.
		rampVolume(voiceL, 0);
		rampVolume(voiceR, 0);
		ducked = true;
	}
}

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
#include "../../vba/gba/Debug.h" // PROFILER_LOG_AUDIO_STARVATION / PROFILER_LOG_DRC

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
 * format, hard L/R pan via device mix, SRC bypass, static full volume.
 ***************************************************************************/
void WutEmulatorAudio::configureVoice(AXVoice* v, int16_t* ringBuf, bool isLeft)
{
	AXVoiceBegin(v);
	AXSetVoiceType(v, 0);

	AXVoiceDeviceMixData tvMix[AX_TV_CHANNELS];
	AXVoiceDeviceMixData drcMix[AX_DRC_CHANNELS];
	memset(tvMix, 0, sizeof(tvMix));
	memset(drcMix, 0, sizeof(drcMix));
	tvMix[0].bus[0].volume = isLeft ? AX_MAX_VOLUME : 0;
	tvMix[1].bus[0].volume = isLeft ? 0 : AX_MAX_VOLUME;
	drcMix[0].bus[0].volume = isLeft ? AX_MAX_VOLUME : 0;
	drcMix[1].bus[0].volume = isLeft ? 0 : AX_MAX_VOLUME;
	AXSetVoiceDeviceMix(v, AX_DEVICE_TYPE_TV, 0, tvMix);
	AXSetVoiceDeviceMix(v, AX_DEVICE_TYPE_DRC, 0, drcMix);

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

	minFrames = AXGetInputSamplesPerFrame() * 3;

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
 * Cheap reset -- called when loading a new game.
 ***************************************************************************/
void WutEmulatorAudio::resetAudio()
{
	writePos = 0;
	queuedFrames = 0;
	voiceRunning = false;
	primed = false;
	rateState = RATE_STATE_NEUTRAL;

	if (voiceL) { AXSetVoiceState(voiceL, AX_VOICE_STATE_STOPPED); AXSetVoiceCurrentOffset(voiceL, 0); }
	if (voiceR) { AXSetVoiceState(voiceR, AX_VOICE_STATE_STOPPED); AXSetVoiceCurrentOffset(voiceR, 0); }
}

void WutEmulatorAudio::stop()
{
	if (voiceL) AXSetVoiceState(voiceL, AX_VOICE_STATE_STOPPED);
	if (voiceR) AXSetVoiceState(voiceR, AX_VOICE_STATE_STOPPED);
	voiceRunning = false;
	primed = false;
}

/****************************************************************************
 * startVoice
 *
 * Resyncs both voices' hardware offset to the oldest sample still queued,
 * then sets them PLAYING.
 ***************************************************************************/
void WutEmulatorAudio::startVoice()
{
	if (!voiceL || !voiceR)
		return;

	uint32_t start = (writePos + RING_FRAMES - (queuedFrames % RING_FRAMES)) % RING_FRAMES;

	AXSetVoiceCurrentOffset(voiceL, start);
	AXSetVoiceCurrentOffset(voiceR, start);
	AXSetVoiceState(voiceL, AX_VOICE_STATE_PLAYING);
	AXSetVoiceState(voiceR, AX_VOICE_STATE_PLAYING);
	voiceRunning = true;
	primed = true;
}

int WutEmulatorAudio::getUnplayed()
{
	if (!primed) return -1;
	return getUnplayedBuffers();
}

bool WutEmulatorAudio::canWrite()
{
	if (appRequest == AppRequest::MENU)
	{
		stop();
		return false;
	}
	return queuedFrames < (uint32_t)MAX_QUEUED_FRAMES;
}

double WutEmulatorAudio::getDynamicRate()
{
	if (turboMode)
	{
		rateState = RATE_STATE_NEUTRAL;
		return RATE_NEUTRAL;
	}

	int unplayed = (int)queuedFrames;

	if (rateState == RATE_STATE_DRAINING && unplayed <= HIGH_RELEASE_FRAMES)
		rateState = RATE_STATE_NEUTRAL;
	else if (rateState == RATE_STATE_FILLING && unplayed >= LOW_RELEASE_FRAMES)
		rateState = RATE_STATE_NEUTRAL;

	if (unplayed > HIGH_WATER_FRAMES)
		rateState = RATE_STATE_DRAINING;
	else if (unplayed < LOW_WATER_FRAMES)
		rateState = RATE_STATE_FILLING;

	int unplayedChunks = getUnplayedBuffers();
	if (unplayedChunks > 12) unplayedChunks = 12;
	PROFILER_LOG_DRC(unplayedChunks, rateState);

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
	queuedFrames += COMMIT_FRAMES;

	if (!voiceRunning && queuedFrames >= (uint32_t)START_LEVEL_FRAMES)
		startVoice();
}

/****************************************************************************
 * frameTick
 *
 * Called from WutAudioDriver's AX frame callback (~3ms), every tick
 * regardless of mode. Retires one tick's worth of frames from queuedFrames
 * while the voices are running
 ***************************************************************************/
void WutEmulatorAudio::frameTick()
{
	if (!voiceRunning)
		return;

	uint32_t frame = AXGetInputSamplesPerFrame();

	if (queuedFrames < minFrames)
	{
		// Starving -- stop outright rather than let the voices loop stale
		// ring content.
		if (voiceL) AXSetVoiceState(voiceL, AX_VOICE_STATE_STOPPED);
		if (voiceR) AXSetVoiceState(voiceR, AX_VOICE_STATE_STOPPED);
		voiceRunning = false;
		PROFILER_LOG_AUDIO_STARVATION();
		return;
	}

	queuedFrames -= frame;
}

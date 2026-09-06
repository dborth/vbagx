/****************************************************************************
 * Visual Boy Advance GX - drivers/ogc
 * Daryl Borth 2008-2026
 * OgcEmulatorAudio.cpp
 *
 * Direct-Queued Audio Driver with Dynamic Rate Control
 ***************************************************************************/
#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "OgcEmulatorAudio.h"
#include "../../vbagx.h"
#include "../../vba/gba/Debug.h"

extern bool turboMode;

/** Dynamic Rate Control (Hysteresis Pitch Bending) **/
#define UNPLAYED_HIGH_WATER 8       // Above this we are building latency, slow down
#define UNPLAYED_HIGH_RELEASE 6     // Stay slow until the queue drains back to here
#define UNPLAYED_LOW_RELEASE 6      // Stay fast until the queue fills back to here
#define UNPLAYED_LOW_WATER 4        // Below this we risk an underrun, speed up
#define UNPLAYED_CRITICAL 1         // At/below this we are one stall away from an audible dropout
#define UNPLAYED_START_LEVEL 6      // Queue at least this many buffers before starting DMA
#define RATE_SLOW_DOWN 1.005        // Emit samples slightly slower to drain the queue
#define RATE_SPEED_UP 0.995         // Emit samples slightly faster to fill the queue
#define RATE_EMERGENCY_SPEED_UP 0.985 // Harder pull-back only when we're on the brink (see getDynamicRate)
#define UNPLAYED_HIGH_CRITICAL 11      // mirrors UNPLAYED_CRITICAL's 3-buffer margin from the opposite hard limit (MAX_QUEUED_BUFFERS=12)
#define RATE_EMERGENCY_SLOW_DOWN 1.015 // mirrors RATE_EMERGENCY_SPEED_UP's 3x-normal-correction magnitude
#define RATE_NEUTRAL 1.0

// The single OgcEmulatorAudio instance currently registered with the DMA
// callback trampoline below. There is only ever one emulator audio backend
// alive at a time.
static OgcEmulatorAudio* instance = nullptr;

/****************************************************************************
 * AudioDMACallback (ISR)
 *
 * Hardware DMA callback trampoline - forwards into the live instance.
 * Executes entirely in interrupt context.
 ***************************************************************************/
void AudioDMACallback()
{
	if (instance)
		instance->dmaCallback();
}

OgcEmulatorAudio::OgcEmulatorAudio() :
	playab(0), nextab(0), dma_started(false), rateState(RATE_STATE_NEUTRAL),
	lastL(0), lastR(0), wasStarved(false)
{
	memset(soundbuffer, 0, sizeof(soundbuffer));
	memset(silence, 0, sizeof(silence));
	memset(fadeBuffer, 0, sizeof(fadeBuffer));
	DCFlushRange(soundbuffer, sizeof(soundbuffer));
	DCFlushRange(silence, sizeof(silence));
	instance = this;
}

OgcEmulatorAudio::~OgcEmulatorAudio()
{
	if (instance == this)
		instance = nullptr;
}

/****************************************************************************
 * buildFadeOutBuffer / applyFadeIn
 *
 * Turn a hard jump to/from zero into a short linear ramp. Both run in
 * interrupt context; the work is a ~96-sample loop, cheap relative to a
 * DMA period.
 ***************************************************************************/
void OgcEmulatorAudio::buildFadeOutBuffer()
{
	s16* out = (s16*)fadeBuffer;
	int const frames = DMA_BYTES / 4; // stereo 16-bit frames per DMA period
	int const n = (frames < FADE_FRAMES) ? frames : FADE_FRAMES;

	for (int i = 0; i < n; i++) {
		out[i * 2]     = (s16)(((s32)lastL * (n - i)) / n);
		out[i * 2 + 1] = (s16)(((s32)lastR * (n - i)) / n);
	}
	for (int i = n; i < frames; i++) {
		out[i * 2] = 0;
		out[i * 2 + 1] = 0;
	}
	DCFlushRange(fadeBuffer, DMA_BYTES);
}

void OgcEmulatorAudio::applyFadeIn(u8* buf)
{
	s16* s = (s16*)buf;
	int const frames = DMA_BYTES / 4;
	int const n = (frames < FADE_FRAMES) ? frames : FADE_FRAMES;

	for (int i = 0; i < n; i++) {
		s[i * 2]     = (s16)(((s32)s[i * 2]     * i) / n);
		s[i * 2 + 1] = (s16)(((s32)s[i * 2 + 1] * i) / n);
	}
	DCFlushRange(buf, DMA_BYTES);
}

// Raw unplayed-buffer count, for callers (vbasupport.cpp's weighted skip-
// pressure model) that want to build their own continuous deficit curve
// rather than react to a fixed threshold. Returns -1 before DMA has
// primed -- there's nothing to starve yet, so callers should treat a
// negative reading as "audio has no opinion right now."
int OgcEmulatorAudio::getUnplayed()
{
	if (!dma_started) return -1;
	return getUnplayedInternal();
}

void OgcEmulatorAudio::dmaCallback()
{
	int unplayed = getUnplayedInternal();

	if (unplayed == 0) {

		if (!wasStarved) {
			PROFILER_LOG_AUDIO_STARVATION();
			buildFadeOutBuffer();
			wasStarved = true;
		}
		AUDIO_InitDMA((u32)fadeBuffer, DMA_BYTES);
	}
	else {
		u8* buf = soundbuffer[playab];

		if (wasStarved) {
			// Coming back from a dry spell: fade the front of this real
			// buffer up from zero instead of snapping straight to it.
			applyFadeIn(buf);
			wasStarved = false;
		}

		AUDIO_InitDMA((u32)buf, DMA_BYTES);

		// Remember the tail of what we just queued, in case the *next*
		// callback finds the ring empty and needs to fade from here.
		s16* s = (s16*)buf;
		int const frames = DMA_BYTES / 4;
		lastL = s[(frames - 1) * 2];
		lastR = s[(frames - 1) * 2 + 1];

		playab = nextIndex(playab);
	}
}

/****************************************************************************
 * resetAudio
 *
 * Called to cleanly kick off the Audio Queue and reset hysteresis state
 ***************************************************************************/
void OgcEmulatorAudio::resetAudio()
{
	nextab = 0;
	playab = 0;
	dma_started = false;
	rateState = RATE_STATE_NEUTRAL;
	wasStarved = false;
	lastL = 0;
	lastR = 0;
}

/****************************************************************************
 * Sound-output contract the VBA core mixes samples through
 ***************************************************************************/
bool OgcEmulatorAudio::canWrite()
{
    if (appRequest == AppRequest::MENU)
    {
        AUDIO_StopDMA();
        resetAudio();
        return false;
    }

    // Pure capacity query, no side effects: is there room in the ring for
    // one more buffer right now?
    return getUnplayed() < MAX_QUEUED_BUFFERS;
}

double OgcEmulatorAudio::getDynamicRate()
{
	// Fast-forward: don't pitch-bend. Turbo audio isn't expected to sound
	// "correct" -- Sound.cpp's own turbo-aware policy in flush_samples()
	// handles keeping the backlog bounded instead.
	if (turboMode) {
		rateState = RATE_STATE_NEUTRAL;
		return RATE_NEUTRAL;
	}

	int unplayed = getUnplayedInternal();

	// Process Hysteresis Release
	if(rateState == RATE_STATE_DRAINING && unplayed <= UNPLAYED_HIGH_RELEASE) {
		rateState = RATE_STATE_NEUTRAL;
	}
	else if(rateState == RATE_STATE_FILLING && unplayed >= UNPLAYED_LOW_RELEASE) {
		rateState = RATE_STATE_NEUTRAL;
	}

	// Process Hysteresis Activation
	if(unplayed > UNPLAYED_HIGH_WATER) {
		rateState = RATE_STATE_DRAINING;
	}
	else if(unplayed < UNPLAYED_LOW_WATER) {
		rateState = RATE_STATE_FILLING;
	}

	PROFILER_LOG_DRC(unplayed, rateState);

	// Return the float multiplier
	// Draining means we need FEWER samples generated per frame.
	// Filling means we need MORE samples generated per frame.
	if(rateState == RATE_STATE_DRAINING) {
		return (unplayed >= UNPLAYED_HIGH_CRITICAL) ? RATE_EMERGENCY_SLOW_DOWN : RATE_SLOW_DOWN;
	}
	else if(rateState == RATE_STATE_FILLING) {
		// Rather than making the everyday 0.5% nudge stronger (and more likely to be
		// audible as pitch wobble during normal play), pull harder only in
		// the narrow window where we're actually about to starve.
		return (unplayed <= UNPLAYED_CRITICAL) ? RATE_EMERGENCY_SPEED_UP : RATE_SPEED_UP;
	}

	return RATE_NEUTRAL;
}

u16* OgcEmulatorAudio::getWriteBuffer()
{
	// Pass the actual DMA-aligned ring buffer address
	return (u16*)soundbuffer[nextab];
}

void OgcEmulatorAudio::commitWrite()
{
	// Publish buffer to the ISR
	DCFlushRange(soundbuffer[nextab], DMA_BYTES);
	nextab = nextIndex(nextab);

	// Handle initial DMA pre-roll and starvation recovery
	if (!dma_started && getUnplayedInternal() >= UNPLAYED_START_LEVEL)
	{
		AUDIO_InitDMA((u32)soundbuffer[playab], DMA_BYTES);
		playab = nextIndex(playab);
		AUDIO_StartDMA();
		dma_started = true;
	}
}

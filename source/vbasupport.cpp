/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * vbasupport.cpp
 *
 * VBA support code
 ***************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <malloc.h>
#include <ogc/lwp_watchdog.h>

#include <sys/stat.h>
#include <errno.h>

#include "vbagx.h"
#include "vbasupport.h"
#include "memmanager.h"
#include "fileop.h"
#include "filebrowser.h"
#include "audio.h"
#include "input.h"
#include "cheatmgr.h"
#include "gameinput.h"
#include "video.h"
#include "menu.h"
#include "utils/decompress.h"
#include "gamesettings.h"
#include "gbaoverrides.h"
#include "gameborder.h"
#include "preferences.h"

#ifdef HW_DOL
#include "drivers/ogc/vm/vm.h"
#include "drivers/ogc/vm/vmpager.h"
#endif

#include "vba/Util.h"
#include "vba/common/Port.h"
#include "vba/common/Patch.h"
#include "vba/gba/Flash.h"
#include "vba/gba/RTC.h"
#include "vba/gba/Sound.h"
#include "vba/gba/GBA.h"
#include "vba/gba/JIT.h"
#include "vba/gb/gb.h"
#include "vba/gb/gbGlobals.h"
#include "vba/gb/gbCheats.h"
#include "vba/gb/gbSound.h"

#include "goomba/goombarom.h"
#include "goomba/goombasav.h"

#define THREAD_SLEEP			50
#define USEC_PER_SEC			1000000
#define FRAME_PERIOD_US			16742	// GBA hardware timing (~59.7275 Hz)
#define MAX_FRAME_SKIP			2		// non-turbo consecutive-skip cap (== old MAX_FRAME_SKIP)
#define TURBO_MAX_FRAME_SKIP	9		// turbo consecutive-skip cap
#define MAX_PACE_DEBT_US		(FRAME_PERIOD_US * MAX_FRAME_SKIP) // cap on banked "behind schedule" debt after a pause/loadstate.

// -- Weighted skip-pressure model --
// Mirrors audio.cpp's UNPLAYED_LOW_RELEASE/UNPLAYED_LOW_WATER breakpoints,
// but kept as its own constants: this is pacing policy deciding whether to
// drop a render, not DRC policy deciding a sample rate, and the two are
// allowed to diverge even though they agree today.
#define AUDIO_DEFICIT_SWEET_SPOT	6		// unplayed >= this: no pressure to skip for audio's sake
#define AUDIO_DEFICIT_LOW_WATER		4		// below this the deficit curve steepens (mirrors the DRC's own emergency tier)
#define SKIP_AUDIO_WEIGHT			0.35f	// audio alone, even at unplayed==0, never quite reaches SKIP_PRESSURE_THRESHOLD
#define SKIP_WALL_WEIGHT			1.0f	// wall-clock alone, fully at the skip cap, always crosses it on its own
#define SKIP_PRESSURE_THRESHOLD		1.0f

static int timerstyle = 0;

// -- Render decision --
// Read directly by GBA.cpp: this single flag gates both this frame's
// systemDrawScreen() call and -- until the next VCOUNT==160 -- the
// following frame's per-scanline CPURenderLine_Wii() calls
bool frameToRender = true;
static int skippedFrames = 0;

// FPS display
static int renderFrameCount = 0;
static int coreFrameCount = 0;
static float renderFPS = 0.0f;
static float coreFPS = 0.0f;
static u64 lastFPS = 0;

// Frame timing
static u64 lastRenderFrameTime = 0;

static u64 start;
int cartridgeType = CARTRIDGE_NONE;
int GBAROMSize = 0;
uint32_t RomIdCode;
char RomTitle[17];

int SunBars = 3;
bool TiltSideways = false;

int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

int emulating = 0;
int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
uint16_t systemGbPalette[24];
uint16_t systemColorMap16[0x10000];

void StopColorizing();

struct EmulatedSystem emulator =
{
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	false,
	0
};

uint32_t systemGetClock(void)
{
    const u64 now = gettime();
    return (uint32_t)(ticks_to_microsecs(now - start) / 1000);
}

void systemGbBorderOn() {}

/* *****************************************************************************
 * Frame pacing, frameskip, & FPS instrumentation
 *****************************************************************************/

/*
 * Clears all pacing/frameskip/FPS state. Called whenever returning from the menu
 */
void systemResetPacer()
{
	FrameTimer = 0;

	if(vmode_60hz) // Video mode matches ROM timing - use vblanks
		timerstyle = 0;
	else // use timing windows with usleep
		timerstyle = 1;

	frameToRender   = true;
	skippedFrames   = 0;

	coreFrameCount = 0;
	coreFPS = 0.0f;

	renderFrameCount = 0;
	renderFPS        = 0.0f;
	lastRenderFrameTime = gettime();
	lastFPS = gettime();
}

float systemGetRenderFPS(void) { return renderFPS; }
float systemGetCoreFPS(void)    { return coreFPS; }

static inline float clampf(float v, float lo, float hi)
{
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

// 0..1: how urgently audio needs this frame's CPU time, from the raw
// unplayed-buffer count. Zero at/above the sweet spot; ramps gently
// through [LOW_WATER, SWEET_SPOT) -- the same zone the DRC's own 0.5%
// nudge covers -- then steepens below LOW_WATER, the same zone the DRC's
// emergency tier watches. Two-segment piecewise-linear rather than a
// single curve so each segment's slope can be tuned independently later.
static float AudioDeficit(int unplayed)
{
	if (unplayed < 0) return 0.0f; // DMA not primed yet -- audio has no opinion

	if (unplayed >= AUDIO_DEFICIT_SWEET_SPOT)
		return 0.0f;

	if (unplayed >= AUDIO_DEFICIT_LOW_WATER)
		return 0.5f * (float)(AUDIO_DEFICIT_SWEET_SPOT - unplayed)
		            / (float)(AUDIO_DEFICIT_SWEET_SPOT - AUDIO_DEFICIT_LOW_WATER);

	return 0.5f + 0.5f * (float)(AUDIO_DEFICIT_LOW_WATER - unplayed)
	                    / (float)AUDIO_DEFICIT_LOW_WATER;
}

// Blend audio urgency and wall-clock urgency into one skip/no-skip call.
// By design, neither alone is normally enough to cross the line -- an
// isolated audio dip with no wall lag, or mild wall lag with a healthy
// buffer, both fall through -- but either a severe reading on one side, or
// moderate corroboration from both, does. This replaces the old
// unconditional "needsFeeding" veto that could fire on any dip below the
// sweet spot regardless of schedule state, which is what let a fast JIT
// core's skip bursts push the ring well past its target band.
static bool SkipPressureCrossed(float audioDeficit, float wallDeficit)
{
	float pressure = (SKIP_AUDIO_WEIGHT * audioDeficit) + (SKIP_WALL_WEIGHT * wallDeficit);
	return pressure >= SKIP_PRESSURE_THRESHOLD;
}

/*
 * Called once per emulated GBA frame (VCOUNT==160), BEFORE the render-or-
 * skip decision is acted on in GBA.cpp. Sets frameToRender for this frame.
 */
void systemFrame()
{
	coreFrameCount++;

	if(cartridgeType == CARTRIDGE_GB) {
		return;
	}

	if (turboMode)
	{
		// Turbo: no real-time throttle at all -- run flat out. Frameskip's
		// only job here is to avoid spending GX_Render()/VSync time on
		// frames nobody's watching; it does not bound the audio backlog
		// (Sound.cpp's own overflow-drop policy in flush_samples() does
		// that, independently, by design -- see audio.cpp's getDynamicRate()).
		if (skippedFrames < TURBO_MAX_FRAME_SKIP)
		{
			skippedFrames++;
			frameToRender = false;
		}
		else
		{
			skippedFrames = 0;
			frameToRender = true;
		}

		// Keep the non-turbo clocks sane for whenever turbo lets go, so we
		// don't inherit stale debt/VBlank-count and read as "behind" the
		// instant turbo turns off.
		FrameTimer = 0;
		return;
	}

	// Non-turbo: we ALWAYS pace to true GBA hardware time (~59.7275Hz),
	// regardless of the user's frameskip preference. "Frameskip off" only
	// answers "may a frame's render be dropped to catch up" -- it must
	// never mean "let a fast JIT core run ahead of real GBA time." Those
	// used to be the same flag; a JIT-fast core with frameskip disabled
	// could run unthrottled (e.g. ~180fps) because the old code skipped
	// pacing entirely rather than just skipping the *render-drop* option.
	int skipFrms = MAX_FRAME_SKIP;

	// Audio urgency, as a continuous 0..1 reading rather than two booleans
	float audioDeficit = AudioDeficit(AudioGetUnplayed());

	if (timerstyle == 0)
	{
		// V-sync-driven pacing. GX_Render() blocks on the real hardware
		// VSync whenever we render, so a fast JIT core is automatically
		// capped at the display's own refresh rate the instant every
		// frame renders -- no separate software throttle is needed here.
		// This naturally satisfies "never exceed true GBA rate" for
		// scenario 2 without any extra code.
		uint32_t pendingFrames = FrameTimer;

		// SKIP PRESSURE: blend how far behind real vblanks we are with
		// how urgently audio needs this frame's CPU time
		float wallDeficit = clampf((float)pendingFrames / (float)skipFrms, 0.0f, 1.0f);
		bool behindSchedule = SkipPressureCrossed(audioDeficit, wallDeficit);

		if (pendingFrames > skipFrms)
		{
			FrameTimer = skipFrms;
			pendingFrames = skipFrms;
		}

		if (GCSettings.gbaFrameskip && behindSchedule && (skippedFrames < skipFrms))
		{
			skippedFrames++;
			frameToRender = false;
			PROFILER_INC(framesSkippedTotal);
			PROFILER_INC(consecutiveSkips);
		}
		else
		{
			// If we were behind but either can't or won't drop a render,
			// forgive the VBlank debt rather than let it sit at max and
			// bias the next frame's decision toward skipping anyway
			if (behindSchedule)
				FrameTimer = GCSettings.gbaFrameskip ? 1 : 0;

			skippedFrames = 0;
			frameToRender = true;
			PROFILER_COMMIT_FRAMESKIP();
		}

		if (FrameTimer > 0)
			FrameTimer--;
	}
	else
	{
		// Time-driven pacing. Nothing here blocks on real hardware, so
		// this is the only thing standing between a fast JIT core and
		// running ahead of true GBA time.
		uint32_t usecSinceLastFrame = diff_usec(lastRenderFrameTime, gettime());

		// SKIP PRESSURE: blend how far behind real time we are with audio urgency
		float wallDeficit = clampf((float)usecSinceLastFrame / (float)MAX_PACE_DEBT_US, 0.0f, 1.0f);
		bool behindSchedule = SkipPressureCrossed(audioDeficit, wallDeficit);

		if(behindSchedule) {
			// 1. Should we drop a render to catch up?
			// We ONLY drop if the skip pressure crossed the threshold (behindSchedule).
			if (GCSettings.gbaFrameskip && behindSchedule && (skippedFrames < skipFrms))
			{
				skippedFrames++;
				frameToRender = false;
				PROFILER_INC(framesSkippedTotal);
				PROFILER_INC(consecutiveSkips);
			}
			else
			{
				skippedFrames = 0;
				frameToRender = true;
				PROFILER_COMMIT_FRAMESKIP();
			}
		}
		else {
			// Ahead of schedule -- LWP-safe micro-sleep loop
			while (usecSinceLastFrame < FRAME_PERIOD_US)
			{
				if (usecSinceLastFrame > THREAD_SLEEP) {
					usleep(THREAD_SLEEP);
					usecSinceLastFrame = diff_usec(lastRenderFrameTime, gettime());
				}
				else {
					break;
				}
			}
		}
	}
}

/****************************************************************************
* System
****************************************************************************/

void systemGbPrint(uint8_t *data,int pages,int feed,int palette, int contrast) {}

static char lastSystemMessage[128];

void systemMessage(int num, const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    vsnprintf(lastSystemMessage, sizeof(lastSystemMessage), msg, args);
    va_end(args);
}

bool MemCPUReadBatteryFile(char * membuffer, int size)
{
	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

	if(size == 512 || size == 0x2000)
	{
		memcpy(eepromData, membuffer, size);
	}
	else
	{
		if(size == 0x20000)
		{
			memcpy(flashSaveMemory, membuffer, 0x20000);
			flashSetSize(0x20000);
		}
		else
		{
			memcpy(flashSaveMemory, membuffer, 0x10000);
			flashSetSize(0x10000);
		}
	}
	return true;
}

extern int gbaSaveType;

int MemCPUWriteBatteryFile(char * membuffer)
{
	int result = 0;
	if(gbaSaveType == 0)
	{
		if(eepromInUse)
			gbaSaveType = 3;
		else
			switch(saveType)
			{
			case 1:
				gbaSaveType = 1;
				break;
			case 2:
				gbaSaveType = 2;
				break;
			}
	}

	if((gbaSaveType) && (gbaSaveType!=5))
	{
		// only save if Flash/Sram in use or EEprom in use
		if(gbaSaveType != 3)
		{
			if(gbaSaveType == 2)
			{
				memcpy(membuffer, flashSaveMemory, flashSize);
				result = flashSize;
			}
			else
			{
				memcpy(membuffer, flashSaveMemory, 0x10000);
				result = 0x10000;
			}
		}
		else
		{
			memcpy(membuffer, eepromData, eepromSize);
			result = eepromSize;
		}
	}
	return result;
}

/****************************************************************************
* LoadBatteryOrState
* Load Battery/State file into memory
* action = FILE_SRAM - Load battery
* action = FILE_STATE - Load state
****************************************************************************/

bool LoadBatteryOrState(char * filepath, int action, bool silent)
{
	bool result = false;
	int offset = 0;
	int device;
		
	if(!FindDevice(filepath, &device))
		return 0;

	AllocSaveBuffer();

	// load the file into savebuffer
	offset = LoadFile(filepath, silent);
			
	if (cartridgeType == CARTRIDGE_GB && goomba_is_sram(savebuffer)) {
		void* cleaned = goomba_cleanup(savebuffer);
		if (cleaned == nullptr) {
			ErrorPrompt(goomba_last_error());
			offset = 0;
		} else {
			if (cleaned != savebuffer) {
				memcpy(savebuffer, cleaned, GOOMBA_COLOR_SRAM_SIZE);
				free(cleaned);
			}
			stateheader* sh = stateheader_for(savebuffer, RomTitle);
			if (sh == nullptr) {
				ErrorPrompt(goomba_last_error());
				offset = 0;
			} else {
				goomba_size_t outsize;
				void* gbc_sram = goomba_extract(savebuffer, sh, &outsize);
				if (gbc_sram == nullptr) {
					ErrorPrompt(goomba_last_error());
					offset = 0;
				} else {
					memcpy(savebuffer, gbc_sram, outsize);
					offset = outsize;
					free(gbc_sram);
				}
			}
		}
	}
	// load savebuffer into VBA memory
	if (offset > 0)
	{
		if(action == FILE_SRAM)
		{
			if(cartridgeType == CARTRIDGE_GB)
				result = MemgbReadBatteryFile((char *)savebuffer, offset);
			else
				result = MemCPUReadBatteryFile((char *)savebuffer, offset);
		}
		else
		{
			result = emulator.emuReadMemState((char *)savebuffer, offset);
		}
	}

	FreeSaveBuffer();

	if(!silent && !result)
	{
		if(offset == 0)
		{
			if(action == FILE_SRAM)
				ErrorPrompt ("Save file not found");
			else
				ErrorPrompt ("State file not found");
		}
		else
		{
			if(action == FILE_SRAM)
				ErrorPrompt ("Invalid save file");
			else
				ErrorPrompt ("Invalid state file");
		}
	}
	return result;
}

bool LoadBatteryOrStateAuto(int action, bool silent)
{
	char filepath[MAXPATHLEN];
	char filepath2[MAXPATHLEN];

	if(!MakeFilePath(filepath, action, ROMFilename, 0))
		return false;

	if (action==FILE_SRAM)
	{
		if (LoadBatteryOrState(filepath, action, SILENT))
			return true;

		if (!GCSettings.AppendAuto)
			return false;

		// look for file with no number or Auto appended
		if(!MakeFilePath(filepath2, action, ROMFilename, -1))
			return false;

		if(LoadBatteryOrState(filepath2, action, silent))
		{
			// rename this file - append Auto
			rename(filepath2, filepath); // rename file (to avoid duplicates)
			return true;
		}
		return false;
	}
	else
	{
		return LoadBatteryOrState(filepath, action, silent);
	}
}

/****************************************************************************
* SaveBatteryOrState
* Save Battery/State file into memory
* action = 0 - Save battery
* action = 1 - Save state
****************************************************************************/

bool SaveBatteryOrState(char * filepath, int action, bool silent)
{
	bool result = false;
	int offset = 0;
	int datasize = 0; // we need the actual size of the data written
	int device;
	
	if(!FindDevice(filepath, &device))
		return 0;

	if(action == FILE_STATE && gameScreenPng.size > 0)
	{
		char screenpath[1024];
		StripExt(screenpath, filepath);
		strcat(screenpath, ".png");
		SaveFile((char *)gameScreenPng.buffer, screenpath, gameScreenPng.size, silent);
	}

	AllocSaveBuffer();

	// put VBA memory into savebuffer, sets datasize to size of memory written
	if(action == FILE_SRAM)
	{
		if(cartridgeType == CARTRIDGE_GB)
			datasize = MemgbWriteBatteryFile((char *)savebuffer);
		else
			datasize = MemCPUWriteBatteryFile((char *)savebuffer);
		
		if (cartridgeType == CARTRIDGE_GB) {
			const char* generic_goomba_error = "Cannot save SRAM in Goomba format (did not load correctly.)";
			// check for goomba sram format
			char* old_sram = (char*)malloc(GOOMBA_COLOR_SRAM_SIZE);
			size_t br = LoadFile(old_sram, filepath, GOOMBA_COLOR_SRAM_SIZE, GOOMBA_COLOR_SRAM_SIZE, true);
			if (br >= GOOMBA_COLOR_SRAM_SIZE && goomba_is_sram(old_sram)) {
				void* cleaned = goomba_cleanup(old_sram);
				if (cleaned == nullptr) {
					ErrorPrompt(generic_goomba_error);
					datasize = 0;
				} else {
					if (cleaned != old_sram) {
						free(old_sram);
						old_sram = (char*)cleaned;
					}
					stateheader* sh = stateheader_for(old_sram, RomTitle);
					if (sh == nullptr) {
						// Game probably doesn't use SRAM
						datasize = 0;
					} else {
						void* new_sram = goomba_new_sav(old_sram, sh, savebuffer, datasize);
						if (new_sram == nullptr) {
							ErrorPrompt(goomba_last_error());
							datasize = 0;
						} else {
							memcpy(savebuffer, new_sram, GOOMBA_COLOR_SRAM_SIZE);
							datasize = GOOMBA_COLOR_SRAM_SIZE;
							free(new_sram);
						}
					}
				}
			}
			free(old_sram);
		}
	}
	else
	{
		if(emulator.emuWriteMemState((char *)savebuffer, SAVEBUFFERSIZE))
			datasize = *((int *)(savebuffer+4)) + 8;
	}

	// write savebuffer into file
	if(datasize > 0)
	{
		offset = SaveFile(filepath, datasize, silent);

		if(offset > 0)
		{
			if(!silent)
				InfoPrompt ("Save successful");
			result = true;
		}
	}
	else
	{
		if(!silent)
			InfoPrompt("No data to save!");
	}

	FreeSaveBuffer();

	return result;
}

bool SaveBatteryOrStateAuto(int action, bool silent)
{
	char filepath[1024];

	if(!MakeFilePath(filepath, action, ROMFilename, 0))
		return false;

	return SaveBatteryOrState(filepath, action, silent);
}
/****************************************************************************
 * Save Screenshot / Preview image
 ***************************************************************************/

bool SavePreviewImg(char * filepath, bool silent)
{
	int device;
	
	if(!FindDevice(filepath, &device))
		return false;

	if(gameScreenPng.size > 0)
	{
		char screenpath[1024];
		strcpy(screenpath, filepath);
		screenpath[strlen(screenpath)] = 0;
		strcat(screenpath, ".png");
		SaveFile((char *)gameScreenPng.buffer, screenpath, gameScreenPng.size, silent);
	}

	if(!silent)
		InfoPrompt ("Save successful");
	return true;
}

/****************************************************************************
* Sound
****************************************************************************/

SoundDriver * systemSoundInit()
{
	soundShutdown();
	return new SoundWii();
}

/****************************************************************************
* systemReadJoypads
****************************************************************************/
bool systemReadJoypads()
{
	UpdatePads();
	return true;
}

uint32_t systemReadJoypad(int which)
{
	if(which == -1) which = 0; // default joypad
	return GetJoy(which);
}

/****************************************************************************
* Motion/Tilt sensor
* Used for games like:
* - Yoshi's Universal Gravitation
* - Kirby's Tilt-N-Tumble
* - Wario Ware Twisted!
*
****************************************************************************/
static int sensorX = 2047;
static int sensorY = 2047;
static int sensorWario = 0x6C0;
static uint8_t sensorDarkness = 0xE8; // total darkness (including daylight on rainy days)
bool CalibrateWario = false;

int systemGetSensorX()
{
	return sensorX;
}

int systemGetSensorY()
{
	return sensorY;
}

int systemGetSensorZ()
{
	if (CalibrateWario) return 0x6C0;
	else return sensorWario;
}

uint8_t systemGetSensorDarkness()
{
	return sensorDarkness;
}

void systemUpdateSolarSensor()
{
	uint8_t sun = 0x0; //sun = 0xE8 - 0xE8 (case 0 and default)

	switch (SunBars)
	{
		case 1:
			sun = 0xE8 -  0xE0;
			break;
		case 2:
			sun = 0xE8 -  0xDA;
			break;
		case 3:
			sun = 0xE8 -  0xD0;
			break;
		case 4:
			sun = 0xE8 -  0xC8;
			break;
		case 5:
			sun = 0xE8 -  0xC0;
			break;
		case 6:
			sun = 0xE8 -  0xB0;
			break;
		case 7:
			sun = 0xE8 -  0xA0;
			break;
		case 8:
			sun = 0xE8 -  0x88;
			break;
		case 9:
			sun = 0xE8 -  0x70;
			break;
		case 10:
			sun = 0xE8 -  0x50;
			break;
		default:
			break;
	}

	struct tm *newtime;
	time_t long_time;

	// regardless of the weather, there should be no sun at night time!
	time(&long_time); // Get time as long integer.
	newtime = localtime(&long_time); // Convert to local time.
	if (newtime->tm_hour > 21 || newtime->tm_hour < 5)
	{
		sun = 0; // total darkness, 9pm - 5am
	}
	else if (newtime->tm_hour > 20 || newtime->tm_hour < 6)
	{
		sun /= 9; // almost total darkness 8pm-9pm, 5am-6am
	}
	else if (newtime->tm_hour > 18 || newtime->tm_hour < 7)
	{
		sun >>= 1;
	}

#ifdef HW_RVL
	// pointing the Gun Del Sol at the ground blocks the sun light,
	// because sometimes you need the shade.
	WPADData *Data = WPAD_Data(0);// first wiimote
	WPADData data = *Data;
	float f = 1.0f;
	if (data.orient.pitch > 0)
	{
		f = 1.0f - (data.orient.pitch/85.0f);
		if (f < 0)
			f = 0;
	}
	sun = int(float(int(sun)) * f);
#endif
	sensorDarkness = 0xE8 - sun;
}

static inline float absf(float f) {
	volatile float tmp = f;
	asm("fabs %0, %0 " : "=f" (tmp) : "f" (tmp));
	return tmp;
}

void systemUpdateMotionSensor()
{
#ifdef HW_RVL
	WPADData *Data = WPAD_Data(0); // first wiimote
	WPADData data = *Data;
	static float OldTiltAngle, OldAvg;
	static bool WasFlat = false;
	float DeltaAngle = 0;

	if (TiltSideways)
	{
		sensorY = 2047+(data.gforce.x*50);
		sensorX = 2047+(data.gforce.y*50);
		TiltAngle = ((-data.orient.pitch) + OldTiltAngle)*0.5f;
		OldTiltAngle = -data.orient.pitch;
	}
	else
	{
		sensorX = 2047-(data.gforce.x*50);
		sensorY = 2047+(data.gforce.y*50);
		TiltAngle = ((data.orient.roll) + OldTiltAngle)*0.5f;
		OldTiltAngle = data.orient.roll;
	}
	DeltaAngle = TiltAngle - OldAvg;
	if (DeltaAngle > 180.0f)
		DeltaAngle -= 360.0f;
	else if (DeltaAngle < -180.0f)
		DeltaAngle += 360.0f;
	OldAvg = TiltAngle;

	if(absf(TiltAngle) < 3.0f)
	{
		WasFlat = true;
		TiltAngle = 0;
	}
	else
	{
		if (WasFlat) TiltAngle *= 0.5f;
		WasFlat = false;
	}

	sensorWario = 0x6C0+DeltaAngle*11;

#endif

	systemUpdateSolarSensor();
}

/****************************************************************************
* systemDrawScreen
****************************************************************************/
static int srcWidth = 0;
static int srcHeight = 0;

void systemDrawScreen()
{
	uint8_t* renderBuffer = pix;

	// Advance pointer by 484 bytes (240 pixels * 2 bpp + 4 byte pitch pad)
	// to skip the uninitialized top row without runtime multiplication stalls
	if (cartridgeType == CARTRIDGE_GBA) {
		renderBuffer += 484;
	}

	GX_Render(srcWidth, srcHeight, renderBuffer);

	renderFrameCount++;
	if (renderFrameCount >= 60)
	{
		uint32_t elapsedUs = diff_usec(lastFPS, gettime());
		if (elapsedUs > 0) {
			renderFPS = ((float)renderFrameCount * (float)USEC_PER_SEC) / (float)elapsedUs;
			coreFPS    = ((float)coreFrameCount * (float)USEC_PER_SEC) / (float)elapsedUs;
		}
		lastFPS = gettime();
		renderFrameCount = 0;
		coreFrameCount = 0;
		PROFILER_LOG_FPS(coreFPS, renderFPS);
	}

	lastRenderFrameTime = gettime();
	PROFILER_MARK_FRAME();
}

static bool ValidGameId(uint32_t id)
{
	if (id == 0)
		return false;
	for (unsigned i = 1u; i <= 4u; ++i)
	{
		uint8_t b = id & 0xFF;
		id >>= 8;
		if (!(b >= 'A' && b <= 'Z') && !(b >= '0' && b <= '9'))
			return false;
	}
	return true;
}

bool IsGameboyGame()
{
	if(cartridgeType == CARTRIDGE_GB && !gbCgbMode && !gbSgbMode)
		return true;
	return false;
}

bool IsGBAGame()
{
	if(cartridgeType == CARTRIDGE_GBA)
		return true;
	return false;
}

static void gbApplyPerImagePreferences()
{
	// Only works for some GB Colour roms
	uint8_t Colour = gbRom[0x143];
	if (Colour == 0x80 || Colour == 0xC0)
	{
		RomIdCode = gbRom[0x13f] | (gbRom[0x140] << 8) | (gbRom[0x141] << 16)
				| (gbRom[0x142] << 24);
		if (!ValidGameId(RomIdCode))
			RomIdCode = 0;
	}
	else
		RomIdCode = 0;
	// Otherwise we need to make up our own code
	RomTitle[15] = '\0';
	RomTitle[16] = '\0';
	if (gbRom[0x143] < 0x7F && gbRom[0x143] > 0x20)
		strncpy(RomTitle, (const char *) &gbRom[0x134], 16);
	else
		strncpy(RomTitle, (const char *) &gbRom[0x134], 15);

	if (RomIdCode == 0)
	{
		if (strcmp(RomTitle, "ZELDA") == 0)
			RomIdCode = LINKSAWAKENING;
		else if (strcmp(RomTitle, "MORTAL KOMBAT") == 0)
			RomIdCode = MK1;
		else if (strcmp(RomTitle, "MORTALKOMBATI&II") == 0)
			RomIdCode = MK12;
		else if (strcmp(RomTitle, "MORTAL KOMBAT II") == 0)
			RomIdCode = MK2;
		else if (strcmp(RomTitle, "MORTAL KOMBAT 3") == 0)
			RomIdCode = MK3;
		else if (strcmp(RomTitle, "MORTAL KOMBAT 4") == 0)
			RomIdCode = MK4;
		else if (strcmp(RomTitle, "SUPER MARIOLAND") == 0)
			RomIdCode = MARIOLAND1;
		else if (strcmp(RomTitle, "MARIOLAND2") == 0)
			RomIdCode = MARIOLAND2;
		else if (strcmp(RomTitle, "METROID2") == 0)
			RomIdCode = METROID2;
		else if (strcmp(RomTitle, "MARBLE MADNESS") == 0)
			RomIdCode = MARBLEMADNESS;
		else if (strcmp(RomTitle, "TMNT FOOT CLAN") == 0)
			RomIdCode = TMNT1;
		else if (strcmp(RomTitle, "TMNT BACK FROM") == 0 || strcmp(RomTitle, "TMNT 2") == 0)
			RomIdCode = TMNT2;
		else if (strcmp(RomTitle, "TMNT3") == 0)
			RomIdCode = TMNT3;
		else if (strcmp(RomTitle, "CASTLEVANIA ADVE") == 0)
			RomIdCode = CVADVENTURE;
		else if (strcmp(RomTitle, "CASTLEVANIA2 BEL") == 0)
			RomIdCode = CVBELMONT;
		else if (strcmp(RomTitle, "CASTLEVANIA") == 0 || strcmp(RomTitle, "CV3 GER") == 0)
			RomIdCode = CVLEGENDS;
		else if (strcmp(RomTitle, "STAR WARS") == 0)
			RomIdCode = SWEP4;
		else if (strcmp(RomTitle, "EMPIRE STRIKES") == 0)
			RomIdCode = SWEP5;
		else if (strcmp(RomTitle, "SRJ DMG") == 0)
			RomIdCode = SWEP6;
		else if (strcmp(RomTitle, "KID DRACULA") == 0)
			RomIdCode = KIDDRACULA;
	}
	// look for matching palettes if a monochrome gameboy game
	// (or if a Super Gameboy game, but the palette will be ignored later in that case)
	if ((Colour != 0x80) && (Colour != 0xC0))
	{
		if (GCSettings.colorize && strcmp(RomTitle, "MEGAMAN") != 0)
			SetPalette(RomTitle);
		else
			StopColorizing();
	}
}

void ResetTiltAndCursor() {
	TiltScreen = false;
	TiltSideways = false;
	CursorVisible = false;
}

/****************************************************************************
 * ApplyPerImagePreferences
 * Apply game specific settings, originally from vba-over.ini
 ***************************************************************************/

static void ApplyPerImagePreferences()
{
	RomIdCode = rom[0xac] | (rom[0xad] << 8) | (rom[0xae] << 16) | (rom[0xaf] << 24);
	RomTitle[0] = '\0';

	char gameId[5];
	gameId[0] = rom[0xac];
	gameId[1] = rom[0xad];
	gameId[2] = rom[0xae];
	gameId[3] = rom[0xaf];
	gameId[4] = '\0';

	int profileIndex = -1;

	// 1. Lookup by CRC32
	uint32_t currentCrc = crc32(0, rom, GBAROMSize);
	for(uint16_t i = 0; i < CRC_COUNT; ++i)
	{
		if(crcTable[i].crc32 == currentCrc)
		{
			profileIndex = crcTable[i].profileIndex;
			break;
		}
	}

	// 2. Lookup by Game ID if CRC match was not found
	if(profileIndex == -1)
	{
		for(uint16_t i = 0; i < GAME_ID_COUNT; ++i)
		{
			if(strncmp(gameIdTable[i].gameId, gameId, 4) == 0)
			{
				profileIndex = gameIdTable[i].profileIndex;
				break;
			}
		}
	}

	// 3. Apply profile settings if a match was discovered
	if(profileIndex >= 0 && profileIndex < PROFILE_COUNT)
	{
		const OverrideProfile& profile = overrideProfiles[profileIndex];

		if (profile.saveType != -1)
			cpuSaveType = profile.saveType;

		if (profile.rtcEnabled != -1)
			rtcEnable(profile.rtcEnabled);

		if (profile.mirroringEnabled != -1)
			mirroringEnable = profile.mirroringEnabled;

		if (profile.flashSize != -1)
			flashSetSize(profile.flashSize);
	}
	else
	{
		// fallback logic / heuristics

		// Pokémon mainline games (Ruby, Sapphire, Emerald, FireRed, LeafGreen)
		if ((gameId[0] == 'A' || gameId[0] == 'B') &&
		    (gameId[1] == 'P' || gameId[1] == 'A') &&
		    (gameId[2] == 'E' || gameId[2] == 'R' || gameId[2] == 'S' || gameId[2] == 'D' || gameId[2] == 'X'))
		{
			cpuSaveType = 3; // FLASH
			flashSetSize(131072);
			rtcEnable(true);
		}
		// Super Mario Advance 4 (Super Mario Bros 3)
		else if (!strncmp(gameId, "AX4", 3))
		{
			cpuSaveType = 3; // FLASH
			flashSetSize(131072);
		}
		// Mother 3
		else if (!strncmp(gameId, "A3U", 3))
		{
			cpuSaveType = 3; // FLASH
			flashSetSize(131072);
		}
		else
		{
			// General Publisher / Sensor checking
			switch (gameId[0])
			{
				case 'F': // Classic NES / Famicom Mini
					cpuSaveType = 1; // EEPROM
					mirroringEnable = true;
					break;
				case 'K': // Accelerometers (Yoshi Topsy-Turvy, Koro Koro Puzzle)
					cpuSaveType = 4; // EEPROM + sensor
					break;
				case 'R': // WarioWare Twisted
				case 'V': // Drill Dozer
					cpuSaveType = 2; // SRAM
					rtcEnableWarioRumble(true);
					break;
				case 'U': // Boktai solar sensor and clock
					cpuSaveType = 1; // EEPROM
					rtcEnable(true);
					break;
			}
		}
	}
}

void LoadPatch()
{
	int patchsize = 0;
	int patchtype = 0;

	AllocSaveBuffer ();

	char patchpath[2][512];
	memset(patchpath, 0, sizeof(patchpath));
	sprintf(patchpath[0], "%s%s.ips",browser.dir,ROMFilename);
	sprintf(patchpath[1], "%s%s.ups",browser.dir,ROMFilename);

	for(; patchtype<2; patchtype++)
	{
		patchsize = LoadFile(patchpath[patchtype], SILENT);

		if(patchsize)
			break;
	}

	if(patchsize > 0)
	{
		ShowAction("Loading patch...");
		// create memory file
		MFILE * mf = memfopen((char *)savebuffer, patchsize);

		if(cartridgeType == CARTRIDGE_GB)
		{
			if(patchtype == 0)
				patchApplyIPS(mf, &gbRom, &gbRomSize);
			else
				patchApplyUPS(mf, &gbRom, &gbRomSize);
		}
		else
		{
			if(patchtype == 0)
				patchApplyIPS(mf, &rom, &GBAROMSize);
			else
				patchApplyUPS(mf, &rom, &GBAROMSize);
		}

		memfclose(mf); // close memory file
	}

	FreeSaveBuffer ();
}

static void ResetGameBorder() {
	gameBorder.clear();
	sgbBorderExtractor.reset(false, false);
}

void InitGameDimensionsAndBorder() {
	gameBorder.clear();

	if(GCSettings.SGBBorder == SGBBORDER_FROMPNG) {
		int bw = 0, bh = 0;
		const char* fallback = (cartridgeType == CARTRIDGE_GBA) ? "defaultgba" : "default";
		uint16_t* borderPixels = BorderManager::load(nullptr, fallback, bw, bh);

		if (borderPixels) {
			gameBorder.setBorder(borderPixels, bw, bh);
		}
	}

	bool wantSgbCapture = (cartridgeType == CARTRIDGE_GB) && gbSgbMode && (GCSettings.SGBBorder == SGBBORDER_FROMGAME);
	sgbBorderExtractor.reset(wantSgbCapture, gameBorder.hasBorder());

	if(cartridgeType == CARTRIDGE_GBA) {
		srcWidth = 240;
		srcHeight = 160;
	}
	else {
		gbBorderOn = (GCSettings.SGBBorder == SGBBORDER_FROMGAME);

		if(gbBorderOn)
		{
			srcWidth = 256;
			srcHeight = 224;
			gbBorderLineSkip = 256;
			gbBorderColumnSkip = 48;
			gbBorderRowSkip = 40;
		}
		else
		{
			srcWidth = 160;
			srcHeight = 144;
			gbBorderLineSkip = 160;
			gbBorderColumnSkip = 0;
			gbBorderRowSkip = 0;
		}
	}

	if (gameBorder.hasBorder()) {
		GX_Render_Init(gameBorder.getWidth(), gameBorder.getHeight());
	} else {
		GX_Render_Init(srcWidth, srcHeight);
	}
}

static bool utilIsZipFile(const char* file)
{
  if(strlen(file) > 4)
    {
      char * p = strrchr(file,'.');
      if(p != nullptr)
        {
          if(strcasecmp(p, ".zip") == 0)
            return true;
        }
	}
	return false;
}

#ifdef HW_DOL
int LoadROMToVM(const char* filepath) {
	int size = 0;
	char zipbuffer[2048];
	int retry = 1;
	int device;

	if(!FindDevice((char*)filepath, &device))
		return 0;

	HaltDeviceCheckingThread();
	HaltParseThread();
	VMPager_CloseFile();

	while(retry) {
		if(!ChangeInterface(device, NOTSILENT))
			break;

		file = fopen(filepath, "rb");
		if(!file) {
			retry = ErrorPromptRetry("Error opening file!");
			continue;
		}

		if (utilIsZipFile(filepath)) {
			size_t readsize = fread(zipbuffer, 1, 32, file);
			if(readsize < 32 || !IsZipFile(zipbuffer)) {
				platform->getFileSystem()->invalidateStorageDevice(device);
				retry = ErrorPromptRetry("Error reading file!");
				fclose(file);
				file = nullptr;
				continue;
			}

			uint32_t uncompSize = ((uint8_t)zipbuffer[22]) |
							 ((uint8_t)zipbuffer[23] << 8) |
							 ((uint8_t)zipbuffer[24] << 16) |
							 ((uint8_t)zipbuffer[25] << 24);

			if(uncompSize > ARAM_SIZE) {
				ErrorPrompt("Compressed ROM file is too large to decompress!");
				fclose(file);
				file = nullptr;
				ResumeDeviceCheckingThread();
				CancelAction();
				return 0;
			}

			VMPager_StartPreload();
			size = UnZipBuffer((unsigned char*)romPtr, ARAM_SIZE);

			if (size > 0 && (uint32_t)size == uncompSize) {
				uint32_t pages = (size + 4095) / 4096;
				VMPager_CommitPageRange(0, pages);
				retry = 0;
			} else {
				retry = ErrorPromptRetry("Error extracting ZIP file!");
			}
			fclose(file);
			file = nullptr;
			VMPager_EndPreload();
		} else {
			fseeko(file, 0, SEEK_END);
			size = ftello(file);
			fseeko(file, 0, SEEK_SET);

			if (size > MAX_GBA_ROM_SIZE) {
				ErrorPrompt("Unsupported file size!");
				fclose(file);
				file = nullptr;
				ResumeDeviceCheckingThread();
				CancelAction();
				return 0;
			}

			VMPager_StartPreload();

			uint32_t preload_size = (size > ARAM_SIZE) ? ARAM_SIZE : size;
			ShowProgress("Loading...", 0, preload_size);

			size_t offset = 0;
			uint8_t* chunk_buf = (uint8_t*)memalign(32, 65536);

			size_t readsize;

			while(offset < preload_size) {
				size_t to_read = preload_size - offset;
				if(to_read > 65536) to_read = 65536;

				readsize = fread(chunk_buf, 1, to_read, file);
				if(readsize <= 0) break;

				memcpy(romPtr + offset, chunk_buf, readsize);

				uint32_t start_page = offset / 4096;
				uint32_t end_page = (offset + readsize - 1) / 4096;
				VMPager_CommitPageRange(start_page, end_page + 1);

				offset += readsize;
				ShowProgress("Loading...", offset, preload_size);
			}

			free(chunk_buf);

			if (offset == size) {
				// <= 16MB file. Everything is in ARAM. We don't need file access so nothing more to do.
				fclose(file);
				file = nullptr;
				VMPager_EndPreload();
				retry = 0;
			} else if (offset == preload_size) {
				// > 16MB file (size > offset, but we loaded 16MB). Preload finished, but more data remains - so we pass a file handle
				FILE* vm_file = file;
				file = nullptr; // isolate the handle exclusively for the VM Pager (it will be responsible to close it)
				VMPager_EndPreloadWithFile(vm_file, size, filepath);
				retry = 0;
			} else {
				fclose(file);
				file = nullptr;
				VMPager_EndPreload();
				retry = ErrorPromptRetry("Error reading uncompressed ROM data!");
			}
		}
	}

	ResumeDeviceCheckingThread();
	CancelAction();

	return size;
}
#endif

bool LoadGBROM()
{
	gbRom = romPtr;
	bios = (uint8_t *)calloc(1,0x100);
	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

	if(!inSz)
	{
		char filepath[1024];

		if(!MakeFilePath(filepath, FILE_ROM))
			return false;

		#ifdef HW_RVL
		gbRomSize = LoadFile ((char *)gbRom, filepath, 0, (1024*1024*8), NOTSILENT);
		#else
		gbRomSize = LoadROMToVM(filepath);
		#endif
	}
	else
	{
		gbRomSize = LoadSzFile(szpath, (unsigned char *)gbRom);
	}
	
	const void* firstRom = gb_first_rom(gbRom, gbRomSize);
	const void* secondRom = gb_next_rom(gbRom, gbRomSize, firstRom);
	if (firstRom != nullptr && firstRom != gbRom) {
		char msgbuf[32];
		char gb_title_buffer[16];
		const void* gbRomPtr;
		for (gbRomPtr = firstRom; gbRomPtr != nullptr; gbRomPtr = gb_next_rom(gbRom, gbRomSize, gbRomPtr)) {
			gb_get_title(gbRomPtr, gb_title_buffer);
			sprintf(msgbuf, "Load %s?", gb_title_buffer);
			if (secondRom == nullptr || YesNoPrompt(msgbuf, true)) {
				gbRomSize = gb_rom_size(gbRomPtr);
				memmove(gbRom, gbRomPtr, gbRomSize);
				break;
			}
		}
		if (gbRomPtr == nullptr) {
			InfoPrompt("No more ROMs found in the file.");
			return false;
		}
	}

	if(gbRomSize <= 0)
		return false;

	if(!gbUpdateSizes()) {
		ErrorPrompt(lastSystemMessage);
		return false;
	}
	return true;
}

static void GBAROMCleanup()
{
	if(vram != nullptr) free(vram);
	if(paletteRAM != nullptr) free(paletteRAM);
	if(internalRAM != nullptr) free(internalRAM);
	if(workRAM != nullptr) free(workRAM);
	if(bios != nullptr) free(bios);
	if(pix != nullptr) free(pix);
	if(oam != nullptr) free(oam);
	if(ioMem != nullptr) free(ioMem);
	vram = nullptr;
	paletteRAM = nullptr;
	internalRAM = nullptr;
	workRAM = nullptr;
	bios = nullptr;
	pix = nullptr;
	oam = nullptr;
	ioMem = nullptr;
}

void RomCleanup()
{
	cartridgeType = CARTRIDGE_NONE;
	GBAROMCleanup(); // cleanup GBA memory
	gbCleanUp(); // cleanup GB memory
	ResetCheats();
	gbRom = nullptr;
	rom = nullptr;
	#ifdef HW_DOL
	VMPager_CloseFile();
	#endif
	srcWidth = 0;
	srcHeight = 0;

	ResetGameBorder();
}

static bool GBAROMAlloc()
{
	workRAM = (uint8_t *)memalign(32, 0x40000);
	bios = (uint8_t *)memalign(32,0x4000);
	internalRAM = (uint8_t *)memalign(32,0x8000);
	paletteRAM = (uint8_t *)memalign(32,0x400);
	vram = (uint8_t *)memalign(32, 0x20000);
	oam = (uint8_t *)memalign(32, 0x400);
	pix = (uint8_t *)memalign(32, 4 * 241 * 162);
	ioMem = (uint8_t *)memalign(32, 0x400);

	if(workRAM == nullptr || bios == nullptr || internalRAM == nullptr ||
		paletteRAM == nullptr || vram == nullptr || oam == nullptr ||
		pix == nullptr || ioMem == nullptr)
	{
		ErrorPrompt("Out of memory!");
		return false;
	}
	return true;
}

static int GBAROMLoad()
{
	GBAROMSize = 0;
	rom = romPtr;

	if(!inSz)
	{
		char filepath[MAXPATHLEN];

		if(!MakeFilePath(filepath, FILE_ROM))
			return 0;

		#ifdef HW_RVL
		GBAROMSize = LoadFile ((char *)rom, filepath, 0, MAX_GBA_ROM_SIZE, NOTSILENT);
		#else
		GBAROMSize = LoadROMToVM(filepath);
		#endif
	}
	else
	{
		GBAROMSize = LoadSzFile(szpath, (unsigned char *)rom);
	}

	if(gb_first_rom(rom, GBAROMSize)) {
		int r = YesNoPrompt("This file contains uncompressed Game Boy (Color) ROMs. Do you want to run these?", true);
		if (r) {
			GBAROMSize = 0;
			rom = nullptr;
			return 2;
		}
	}

	if(!GBAROMAlloc()) {
		return 0;
	}

	if(GBAROMSize) {
		flashInit();
		eepromInit();
		CPUUpdateRenderBuffers( true );
		return 1;
	}
	return 0;
}

void InitGBGame() {
	gbEmulatorType = GCSettings.GBHardware;
	gbGetHardwareType();
	gbApplyPerImagePreferences();

	gbSoundReset();
	gbSoundSetDeclicking(true);
	gbReset();
}

bool LoadVBAROM()
{
	int newCartridgeType = CARTRIDGE_NONE;

	// image type (checks file extension)
	if(utilIsGBAImage(browserList[browser.selIndex].filename))
		newCartridgeType = CARTRIDGE_GBA;
	else if(utilIsGBImage(browserList[browser.selIndex].filename))
		newCartridgeType = CARTRIDGE_GB;
	else if(utilIsZipFile(browserList[browser.selIndex].filename))
	{
		// we need to check the file extension of the first file in the archive
		char * zippedFilename = GetFirstZipFilename ();

		if(zippedFilename == nullptr) // loading the file failed
		{
			ErrorPrompt("Empty or invalid ZIP file!");
			return false;
		}

		if(utilIsGBAImage(zippedFilename))
		{
			newCartridgeType = CARTRIDGE_GBA;
		}
		else if(utilIsGBImage(zippedFilename))
		{
			newCartridgeType = CARTRIDGE_GB;
		}
		else
		{
			ErrorPrompt("Unrecognized file extension!");
			free(zippedFilename);
			return false;
		}
		free(zippedFilename);
	}

	// leave before we do anything
	if(newCartridgeType != CARTRIDGE_GB && newCartridgeType != CARTRIDGE_GBA)
	{
		// Not zip gba agb gbc cgb sgb gb mb bin elf or dmg!
		ErrorPrompt("Unrecognized file extension!");
		return false;
	}

	RomCleanup();

	cartridgeType = newCartridgeType;
	int loaded = 0;

	if(cartridgeType == CARTRIDGE_GBA)
	{
		emulator = GBASystem;
		loaded = GBAROMLoad();
		cpuSaveType = 0;
		if (loaded == 2) {
			loaded = 0;
			cartridgeType = CARTRIDGE_GB; // GB ROM within GBA rom - falls through to load below
		}
	}

	if (cartridgeType == CARTRIDGE_GB)
	{
		emulator = GBSystem;
		loaded = LoadGBROM();
	}

	if(!loaded) {
		RomCleanup();
		return false;
	}

	LoadPatch();
	soundInit();

	if (cartridgeType == CARTRIDGE_GB)
	{
		InitGBGame();
	}
	else
	{
		// Set defaults
		cpuSaveType = 0; // automatic
		flashSetSize(0x10000); // 64K saves
		rtcEnable(false);
		mirroringEnable = false;

		// Apply preferences specific to this game
		ApplyPerImagePreferences();
		doMirroring(mirroringEnable);
		soundReset();
		CPUInit(nullptr, false);
		CPUReset();
	}

	soundInit();

	emulating = 1;
	// Start system clock
	start = gettime();
	return true;
}

/****************************************************************************
* Palette
****************************************************************************/

void InitialisePalette()
{
	int i;
	// Build GBPalette
	for( i = 0; i < 24; )
	{
		if (GCSettings.BasicPalette == BASICPALETTE_GREEN) //Greenish color
		{
			systemGbPalette[i++] = (0x1c) | (0x1e << 5) | (0x1c << 10);
			systemGbPalette[i++] = (0x10) | (0x17 << 5) | (0x0b << 10);
			systemGbPalette[i++] = (0x27) | (0x0c << 5) | (0x0a << 10);
		}
		else	// Monochrome color
		{
			systemGbPalette[i++] = (0x1f) | (0x1f << 5) | (0x1f << 10);
			systemGbPalette[i++] = (0x15) | (0x15 << 5) | (0x15 << 10);
			systemGbPalette[i++] = (0x0c) | (0x0c << 5) | (0x0c << 10);
		}
		systemGbPalette[i++] = 0;
	}
	// Set palette mapping - Configured for RGB555 / GX_TF_RGB5A3 (MSB = 1)
	systemRedShift = 10;
	systemGreenShift = 5;
	systemBlueShift = 0;
	for(i = 0; i < 0x10000; i++)
	{
		systemColorMap16[i] =
			((i & 0x1f) << systemRedShift) |
			(((i & 0x3e0) >> 5) << systemGreenShift) |
			(((i & 0x7c00) >> 10) << systemBlueShift);
	}
}

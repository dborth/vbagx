/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * input.h
 *
 * Wii/Gamecube controller management
 ***************************************************************************/

#ifndef _INPUT_H_
#define _INPUT_H_

#include <stdint.h>
#include "drivers/InputData.h"

#define MAXJP 				10 // # of mappable controller buttons

#define VBA_BUTTON_A		1
#define VBA_BUTTON_B		2
#define VBA_BUTTON_SELECT	4
#define VBA_BUTTON_START	8
#define VBA_RIGHT			16
#define VBA_LEFT			32
#define VBA_UP				64
#define VBA_DOWN			128
#define VBA_BUTTON_R		256
#define VBA_BUTTON_L		512
#define VBA_SPEED			1024
#define VBA_CAPTURE			2048

extern int rumbleRequest[4];
extern int playerMapping[4];
extern uint32_t btnmap[INPUT_HW_MAX][MAXJP];

void ResetControls(int wc = -1);
void systemGameRumble(int RumbleForFrames);
void systemGameRumbleOnlyFor(int OnlyRumbleForFrames);
uint32_t GetJoy(int which);
bool isMenuRequested();

//! The Game Boy Advance buttons the user's Button Mappings make of what a kind of hardware (an INPUT_HW_* value) holds
uint32_t MappedButtons(int hw, uint32_t held);

//! The loaded game has its own Wii/GameCube control scheme (see "Match Wii Controls" in the README)
bool GameHasWiiControls();
//! "Match Wii Controls" is on AND the loaded game has such a scheme: the game ignores Button Mappings
bool WiiControlsOverrideActive();

#endif

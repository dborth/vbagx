/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 * softdev 2007
 *
 * video.cpp
 *
 * Video routines
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video.h"
#include "fileop.h"
#include "memmanager.h"
#include "menu.h"
#include "drivers/ogc/OgcEmulatorVideo.h"
#include "utils/pngcodec.h"

u8* texturemem = nullptr;

s32 CursorX, CursorY;
bool CursorVisible;
bool CursorValid;
bool TiltScreen = false;
float TiltAngle = 0;

GameScreenPng gameScreenPng;

/****************************************************************************
 * ClearScreenshot
 ***************************************************************************/
void ClearScreenshot()
{
	if(gameScreenPng.buffer) {
		memspace_free(gameScreenPng.buffer);
		gameScreenPng.buffer = nullptr;
	}
	gameScreenPng.size = 0;
}

/****************************************************************************
 * TakeScreenshot
 *
 * Copies the current texturemem screen into a PNG buffer
 ***************************************************************************/
void TakeScreenshot(u8 * gameTexture)
{
	AllocSaveBuffer();
	OgcEmulatorVideo* emulatorVideo = static_cast<OgcEmulatorVideo*>(platform->getVideo()->getEmulatorVideo());
	emulatorVideo->untileRGB5A3ToRGB24(gameTexture, gameScreenPng.width, gameScreenPng.height, savebuffer);
	u32 size = 0;
	gameScreenPng.buffer = EncodePNGFromRGB24(gameScreenPng.width, gameScreenPng.height, savebuffer, 0, &size);
	gameScreenPng.size = (int) size;
	FreeSaveBuffer();
}

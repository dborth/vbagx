/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 * softdev 2007
 *
 * video.h
 *
 * Video routines
 ***************************************************************************/

#ifndef _VIDEO_H_
#define _VIDEO_H_

#include <stdint.h>

#define TEX_WIDTH 640
#define TEX_HEIGHT 480
#define TEXTUREMEM_SIZE 	TEX_WIDTH*TEX_HEIGHT*2

void TakeScreenshot(uint8_t * gameTexture);
void ClearScreenshot();

extern int32_t CursorX, CursorY;
extern bool CursorVisible;
extern bool CursorValid;
extern bool TiltScreen;
extern float TiltAngle;

typedef struct
{
	uint8_t * buffer;
	int size;
	int width;
	int height;
	float scaleX;
	float scaleY;
	int xoffset;
	int yoffset;
} GameScreenPng;

extern uint8_t * texturemem;
extern GameScreenPng gameScreenPng;

#endif

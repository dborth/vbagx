/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * OgcEmulatorVideo.cpp
 ***************************************************************************/
#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ogc/machine/processor.h>

#include "OgcEmulatorVideo.h"
#include "OgcVideoDriver.h"
#include "../../video.h"
#include "videofilters.h"

/****************************************************************************
 * Scanline Support Functions
 ***************************************************************************/
void OgcEmulatorVideo::initScanlineTexture()
{

}

void OgcEmulatorVideo::setupScanlineFilterTEV()
{

}

/****************************************************************************
 * Scaler Support Functions
 ***************************************************************************/
void OgcEmulatorVideo::drawInit()
{

}

void OgcEmulatorVideo::drawSquare()
{

}

/****************************************************************************
 * resetVideo
 *
 * Reset the video/rendering mode for the emulator rendering
 ***************************************************************************/
void OgcEmulatorVideo::resetVideo()
{

}

// Un-swizzles a 4x4-tiled GX_TF_RGB5A3 texture into dst (RGB24)
void OgcEmulatorVideo::untileRGB5A3ToRGB24(const void * tiledTexture, int width, int height, uint8_t* dst)
{
	int padded_width = (width + 3) & ~3;
	const u16 * tex16 = (const u16 *) tiledTexture;

	for(int y = 0; y < height; y++) {
		int tile_y = y / 4;
		int in_tile_y = y % 4;
		for(int x = 0; x < width; x++) {
			int tile_x = x / 4;
			int in_tile_x = x % 4;

			int tex_pixel_idx = (tile_y * (padded_width / 4) + tile_x) * 16 + (in_tile_y * 4 + in_tile_x);
			u16 color = tex16[tex_pixel_idx];

			// RGB555 format
			u8 r = (color >> 10) & 0x1F;
			u8 g = (color >> 5) & 0x1F;
			u8 b = color & 0x1F;

			int out_idx = (y * width + x) * 3;
			dst[out_idx]     = (r << 3) | (r >> 2);
			dst[out_idx + 1] = (g << 3) | (g >> 2);
			dst[out_idx + 2] = (b << 3) | (b >> 2);
		}
	}
}

void OgcEmulatorVideo::readFrameRGB24(uint8_t* dst)
{

}

void OgcEmulatorVideo::init(VideoDriver* driver)
{
	videoDriver = static_cast<OgcVideoDriver*>(driver);
}

/****************************************************************************
 * presentFrame
 ***************************************************************************/
void OgcEmulatorVideo::presentFrame(int width, int height)
{

}

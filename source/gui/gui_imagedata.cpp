/****************************************************************************
 * libwiigui
 *
 * Daryl Borth 2009
 *
 * gui_imagedata.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"

/**
 * Constructor for the GuiImageData class.
 */
GuiImageData::GuiImageData(const u8 * i, int maxw, int maxh)
{
	data = NULL;
	width = 0;
	height = 0;

	if(i)
		data = DecodePNG(i, &width, &height, data, maxw, maxh);
}

GuiImageData::GuiImageData(const u8 * i, u8 * dst, int maxw, int maxh)
{
	data = NULL;
	width = 0;
	height = 0;

	if(i)
		data = DecodePNG(i, &width, &height, dst, maxw, maxh);
}

/**
 * Destructor for the GuiImageData class.
 */
GuiImageData::~GuiImageData()
{
	if(data)
	{
		mem1_free(data);
		data = NULL;
	}
}

u8 * GuiImageData::GetImage()
{
	return data;
}

int GuiImageData::GetWidth()
{
	return width;
}

int GuiImageData::GetHeight()
{
	return height;
}

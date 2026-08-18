/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 *
 * GuiImageData.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "Gui.h"

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

u8 * GuiImageData::getImage()
{
	return data;
}

int GuiImageData::getWidth()
{
	return width;
}

int GuiImageData::getHeight()
{
	return height;
}

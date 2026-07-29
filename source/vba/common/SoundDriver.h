// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2008 VBA-M development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef __VBA_SOUND_DRIVER_H__
#define __VBA_SOUND_DRIVER_H__

#include "Types.h"

/**
 * Sound driver abstract interface for the core to use to output sound.
 * Subclass this to implement a new sound driver.
 */
class SoundDriver
{
public:
	virtual double getDynamicRate() = 0;
	virtual bool canWrite() = 0;
	virtual u16* getWriteBuffer() = 0;
	virtual void commitWrite() = 0;
	virtual ~SoundDriver() = default;
};

#endif // __VBA_SOUND_DRIVER_H__

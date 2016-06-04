/* goombarom.h - functions to find uncompressed Game Boy ROM images
stored within a larger file (e.g. Goomba Color ROMs, TAR archives)

Copyright (C) 2014 libertyernie

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

When compiling in Visual Studio, set all goombarom files to compile
as C++ code (Properties -> C/C++ -> Advanced -> Compile As.)
*/

#include <stddef.h>

/* Finds the (expected) size of a Game Boy ROM, given a pointer to the start
of the ROM image. */
unsigned int gb_rom_size(const void* rom_start);

/* Finds the first Game Boy ROM in the given data block by looking for the
Nintendo logo that shows when you turn the Game Boy on. If no valid data is
found, this method will return NULL. */
const void* gb_first_rom(const void* data, size_t length);

/* Returns a pointer to the next Game Boy ROM in the data. If the location
where the next ROM would be does not contain a valid Nintendo logo at 0x104,
this method will return NULL. */
const void* gb_next_rom(const void* data, size_t length, const void* first_rom);

/* Returns a copy of the title from the ROM header. If buffer is NULL, the
string will be allocated in an internal 16-byte buffer which will be
overwritten later. If buffer is not NULL, the title will be copied to buffer,
and buffer will be returned. */
const char* gb_get_title(const void* rom, char* buffer);

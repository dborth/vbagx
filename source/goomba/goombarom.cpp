/* goombarom.c - functions to find uncompressed Game Boy ROM images
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

#include <stdlib.h>
#include <string.h>
#include "goombarom.h"

const char NINTENDO_LOGO_GB[48] = { 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E };
char gb_title_buffer[16];

/* Finds the (expected) size of a Game Boy ROM, given a pointer to the start
of the ROM image. */
unsigned int gb_rom_size(const void* rom_start) {
	const char* ptr = (const char*)rom_start;
	return 0x8000 << ptr[0x148];
}

/* Finds the first Game Boy ROM in the given data block by looking for the
Nintendo logo that shows when you turn the Game Boy on. If no valid data is
found, this method will return NULL. */
const void* gb_first_rom(const void* data, size_t length) {
	const char* ptr = (const char*)data;
	const char* end = ptr + length;
	int logo_pos = 0;
	while (ptr < end) {
		if (*ptr == NINTENDO_LOGO_GB[logo_pos]) {
			// match
			logo_pos++;
			if (logo_pos == 48) { // matched all of GB logo - on last byte (0x133)
				return ptr - 0x133;
			}
		} else {
			// no match, try again
			if (logo_pos > 0) {
				ptr -= logo_pos;
				logo_pos = 0;
			}
		}
		ptr++;
	}
	return NULL;
}

/* Returns a pointer to the next Game Boy ROM in the data. If no valid Game
Boy ROM is found between *data and *data+length, starting at *rom, this method
will return NULL.*/
const void* gb_next_rom(const void* data, size_t length, const void* rom) {
	size_t diff = (const char*)rom - (const char*)data;
	if (diff > length) {
		//fprintf(stderr, "*data and *rom are farther apart than length param\n");
		return NULL;
	}
	size_t effective_length = length - diff;
	if (effective_length <= 0x200) {
		return NULL;
	}
	return gb_first_rom((const char*)rom + 0x134, effective_length - 0x134);
}

const char* gb_get_title(const void* rom, char* buffer) {
	char* title = buffer != NULL ? buffer : gb_title_buffer;
	title[15] = '\0';
	memcpy(title, (char*)rom + 0x134, 15);
	return title;
}

#ifndef LIBINPUT_INTERNAL_H
#define LIBINPUT_INTERNAL_H

/* Trimmed from libwiiinput: only what the pen tablet and uDraw drivers need. */

#include <gctypes.h>
#include <stdbool.h>
#include "input_tablet.h"

bool inpw_udraw_pop(tb_sample *out);
bool inpw_udraw_state(tb_sample *out);
bool inpw_udraw_info(tb_info *info);
void inpw_udraw_extent(s32 *w, s32 *h);

#endif

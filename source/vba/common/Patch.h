#ifndef PATCH_H
#define PATCH_H

#include "memfile.h"
#include "Types.h"

bool applyPatch(const char *patchname, u8 **rom, int *size);
bool patchApplyIPS(MFILE * f, u8 **r, int *s);
bool patchApplyUPS(MFILE * f, u8 **rom, int *size);
bool patchApplyPPF(MFILE *f, u8 **rom, int *size);

#endif // PATCH_H

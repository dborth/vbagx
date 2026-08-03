/**
 * vm.h
 * Copyright (C) 2012  tueidj
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
**/

#ifndef _VM_H_
#define _VM_H_
#ifdef HW_DOL
#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void* VM_Init(u32 VMSize, u32 MEMSize);
extern void* VM_GetBase(void);
extern void VM_Clear(void);
extern void VM_Deinit(void);

#ifdef __cplusplus
}
#endif

#endif
#endif

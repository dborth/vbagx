/**
 * vmdsihandler.s - DSI Handler required by libOGC for VM
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
#include <ppc-asm.h>
#include <ogc/machine/asm.h>

.extern vm_dsi_handler
.extern default_exceptionhandler

FUNC_START(vm_dsi_handler_stub)

	stwu        sp,-EXCEPTION_FRAME_END(sp)
	stw         r6,GPR6_OFFSET(sp)
	stw         r7,GPR7_OFFSET(sp)
	stw         r8,GPR8_OFFSET(sp)
	stw         r9,GPR9_OFFSET(sp)
	stw         r10,GPR10_OFFSET(sp)
	stw         r11,GPR11_OFFSET(sp)
	stw         r12,GPR12_OFFSET(sp)
	mfdsisr     r4
	mfmsr       r3
	ori         r3,r3,MSR_RI
	mtmsr       r3
	
	mfdsisr		r3
	mfdar		r4
	bl          vm_dsi_handler
	
	# check if it was handled correctly
	cmplwi      r3,0

	lwz         r6,GPR6_OFFSET(sp)
	lwz         r7,GPR7_OFFSET(sp)
	lwz         r8,GPR8_OFFSET(sp)
	lwz         r9,GPR9_OFFSET(sp)
	lwz         r10,GPR10_OFFSET(sp)
	lwz         r11,GPR11_OFFSET(sp)
	lwz         r12,GPR12_OFFSET(sp)

	# clear MSR_RI
	mfmsr       r3
	rlwinm      r3,r3,0,31,29
	mtmsr       r3

	bne         1f
	
	# jump to libogc's default handler
	addi        sp,sp,EXCEPTION_FRAME_END
	b           default_exceptionhandler

1:	
	lwz         r3,CR_OFFSET(sp)
	lwz         r4,LR_OFFSET(sp)
	lwz         r5,CTR_OFFSET(sp)
	lwz         r0,XER_OFFSET(sp)
	mtcr        r3
	mtlr        r4
	mtctr       r5
	mtxer       r0
	lwz         r0,GPR0_OFFSET(sp)
	lwz         r5,GPR5_OFFSET(sp)
	
	lwz         r3,SRR0_OFFSET(sp)
	lwz         r4,SRR1_OFFSET(sp)
	mtsrr0      r3
	mtsrr1      r4
	lwz         r3,GPR3_OFFSET(sp)
	lwz         r4,GPR4_OFFSET(sp)
	lwz         sp,GPR1_OFFSET(sp)
	rfi
FUNC_END(vm_dsi_handler_stub)

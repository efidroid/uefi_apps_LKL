/* Copyright (C) 2012-2015 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <AsmMacroIoLib.h>
#include "ucontext_offsets.h"

/* int getcontext (ucontext_t *ucp) */

ASM_FUNC(getcontext)
	/* No need to save r0-r3, d0-d7, or d16-d31.  */
	add	r1, r0, #MCONTEXT_ARM_R4
	stmia   r1, {r4-r11}

	/* Save R13 separately as Thumb can't STM it.  */
	str     r13, [r0, #MCONTEXT_ARM_SP]
	str     r14, [r0, #MCONTEXT_ARM_LR]
	/* Return to LR */
	str     r14, [r0, #MCONTEXT_ARM_PC]
	/* Return zero */
	mov     r2, #0
	str     r2, [r0, #MCONTEXT_ARM_R0]

	/* Save ucontext_t * across the next call.  */
	mov	r4, r0

	/* Store FP regs.  Much of the FP code is copied from arm/setjmp.S.  */

	add	r0, r4, #UCONTEXT_REGSPACE

	/* Store the VFP registers.
	   Don't use VFP instructions directly because this code
	   is used in non-VFP multilibs.  */
	/* Following instruction is vstmia r0!, {d8-d15}.  */
	stc     p11, cr8, [r0], #64
	/* Store the floating-point status register.  */
	/* Following instruction is vmrs r1, fpscr.  */
	mrc     p10, 7, r1, cr1, cr0, 0
	str     r1, [r0], #4

#ifndef SETCONTEXT_STORE_IWMMXT
	b     .Lno_iwmmxt
#endif

	/* Save the call-preserved iWMMXt registers.  */
	/* Following instructions are wstrd wr10, [r0], #8 (etc.)  */
	stcl    p1, cr10, [r0], #8
	stcl    p1, cr11, [r0], #8
	stcl    p1, cr12, [r0], #8
	stcl    p1, cr13, [r0], #8
	stcl    p1, cr14, [r0], #8
	stcl    p1, cr15, [r0], #8
.Lno_iwmmxt:

	/* Restore the clobbered R4 and LR.  */
	ldr	r14, [r4, #MCONTEXT_ARM_LR]
	ldr	r4, [r4, #MCONTEXT_ARM_R4]

	mov	r0, #0

	bx r14

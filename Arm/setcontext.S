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

/* ??? Needs more rearrangement for the LDM to handle thumb mode.  */
#include <AsmMacroIoLib.h>
#include "ucontext_offsets.h"

/* int setcontext (const ucontext_t *ucp) */

ASM_FUNC(setcontext)
	mov	r4, r0
	add	r0, r0, #UCONTEXT_REGSPACE

	/* Restore the VFP registers.  Copied from arm/__longjmp.S.  */

	/* Following instruction is vldmia r0!, {d8-d15}.  */
	ldc     p11, cr8, [r0], #64
	/* Restore the floating-point status register.  */
	ldr     r1, [r0], #4
	/* Following instruction is fmxr fpscr, r1.  */
	mcr     p10, 7, r1, cr1, cr0, 0

#ifndef SETCONTEXT_STORE_IWMMXT
	b     .Lno_iwmmxt_sc
#endif

	/* Restore the call-preserved iWMMXt registers.  */
	/* Following instructions are wldrd wr10, [r0], #8 (etc.)  */
	ldcl    p1, cr10, [r0], #8
	ldcl    p1, cr11, [r0], #8
	ldcl    p1, cr12, [r0], #8
	ldcl    p1, cr13, [r0], #8
	ldcl    p1, cr14, [r0], #8
	ldcl    p1, cr15, [r0], #8
.Lno_iwmmxt_sc:

	/* Loading r0-r3 makes makecontext easier.  */
	add     r14, r4, #MCONTEXT_ARM_R0
	ldmia   r14, {r0-r12}
	ldr     r13, [r14, #(MCONTEXT_ARM_SP - MCONTEXT_ARM_R0)]
	add     r14, r14, #(MCONTEXT_ARM_LR - MCONTEXT_ARM_R0)
	ldmia   r14, {r14, pc}

	/* Called when a makecontext() context returns.  Start the
	   context in R4 or fall through to exit().  */
ASM_FUNC(__startcontext)
	movs    r0, r4
	bne     setcontext

	@ New context was 0 - exit
	b       exit

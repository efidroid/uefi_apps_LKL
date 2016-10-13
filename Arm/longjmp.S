/* longjmp for ARM.
   Copyright (C) 1997-2016 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include <AsmMacroIoLib.h>

/* Register list for a ldm/stm instruction to load/store
   the general registers from a __jmp_buf.  */
# define JMP_BUF_REGLIST	{v1-v6, sl, fp}

/* __longjmp(jmpbuf, val) */

ASM_FUNC (lkl_longjmp)
	mov	ip, r0

	ldr	a4, [ip], #4
	ldr	r4, [ip], #4

	mov	sp, a4
	mov	lr, r4
	ldmia	ip!, JMP_BUF_REGLIST

#if 0
#ifdef __SOFTFP__
	tst	a4, #HWCAP_ARM_VFP
	beq	.Lno_vfp
#endif

	/* Restore the VFP registers.  */
	/* Following instruction is vldmia ip!, {d8-d15}.  */
	sfi_breg r12, \
	ldc	p11, cr8, [\B], #64
.Lno_vfp:
#endif

	movs	r0, r1		/* get the return value in place */
	it	eq
	moveq	r0, #1		/* can't let setjmp() return zero! */

	bx lr

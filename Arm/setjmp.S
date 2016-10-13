/* setjmp for ARM.
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

ASM_FUNC (lkl_setjmp)
	mov	ip, r0

	/* Save sp and lr */
	str	sp, [ip], #4
	str	lr, [ip], #4

	/* Save registers */
	stmia	ip!, JMP_BUF_REGLIST

#if 0
#ifdef __SOFTFP__
	tst	a3, #HWCAP_ARM_VFP
	beq	.Lno_vfp
#endif

	/* Store the VFP registers.
	   Don't use VFP instructions directly because this code
	   is used in non-VFP multilibs.  */
	/* Following instruction is vstmia ip!, {d8-d15}.  */
	stc	p11, cr8, [\B], #64
.Lno_vfp:
#endif

	/* Make a tail call to __sigjmp_save; it takes the same args.  */
	//B	PLTJMP(C_SYMBOL_NAME(__sigjmp_save))
    mov r0, #0
    bx lr

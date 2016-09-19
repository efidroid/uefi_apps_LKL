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

/* int swapcontext (ucontext_t *oucp, const ucontext_t *ucp) */

ASM_FUNC(swapcontext)

	/* Have getcontext() do most of the work then fix up
	   LR afterwards.  Save R3 to keep the stack aligned.  */
	push	{r0,r1,r3,r14}

	bl	getcontext
	mov	r4, r0

	pop	{r0,r1,r3,r14}

	/* Exit if getcontext() failed.  */
	cmp 	r4, #0
	itt	ne
	movne	r0, r4
	bxne r14

	/* Fix up LR and the PC.  */
	str	r13,[r0, #MCONTEXT_ARM_SP]
	str	r14,[r0, #MCONTEXT_ARM_LR]
	str	r14,[r0, #MCONTEXT_ARM_PC]

	/* And swap using swapcontext().  */
	mov	r0, r1
	b	setcontext

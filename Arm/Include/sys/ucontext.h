/* Copyright (C) 1998-2016 Free Software Foundation, Inc.
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

/* System V/ARM ABI compliant context switching support.  */

#ifndef _SYS_UCONTEXT_H
#define _SYS_UCONTEXT_H	1

#include <sys/types.h>

# define _SIGSET_NWORDS	(1024 / (8 * sizeof (unsigned long int)))
typedef struct
  {
    unsigned long int __val[_SIGSET_NWORDS];
  } __sigset_t;

/* Alternate, preferred interface.  */
typedef struct sigaltstack
  {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
  } stack_t;

/*
 * Signal context structure - contains all info to do with the state
 * before the signal handler was invoked.  Note: only add new entries
 * to the end of the structure.
 */
struct sigcontext {
	unsigned long trap_no;
	unsigned long error_code;
	unsigned long oldmask;
	unsigned long arm_r0;
	unsigned long arm_r1;
	unsigned long arm_r2;
	unsigned long arm_r3;
	unsigned long arm_r4;
	unsigned long arm_r5;
	unsigned long arm_r6;
	unsigned long arm_r7;
	unsigned long arm_r8;
	unsigned long arm_r9;
	unsigned long arm_r10;
	unsigned long arm_fp;
	unsigned long arm_ip;
	unsigned long arm_sp;
	unsigned long arm_lr;
	unsigned long arm_pc;
	unsigned long arm_cpsr;
	unsigned long fault_address;
};

typedef int greg_t;

/* Number of general registers.  */
#define NGREG	18

/* Container for all general registers.  */
typedef greg_t gregset_t[NGREG];

/* Number of each register is the `gregset_t' array.  */
enum
{
  REG_R0 = 0,
#define REG_R0	REG_R0
  REG_R1 = 1,
#define REG_R1	REG_R1
  REG_R2 = 2,
#define REG_R2	REG_R2
  REG_R3 = 3,
#define REG_R3	REG_R3
  REG_R4 = 4,
#define REG_R4	REG_R4
  REG_R5 = 5,
#define REG_R5	REG_R5
  REG_R6 = 6,
#define REG_R6	REG_R6
  REG_R7 = 7,
#define REG_R7	REG_R7
  REG_R8 = 8,
#define REG_R8	REG_R8
  REG_R9 = 9,
#define REG_R9	REG_R9
  REG_R10 = 10,
#define REG_R10	REG_R10
  REG_R11 = 11,
#define REG_R11	REG_R11
  REG_R12 = 12,
#define REG_R12	REG_R12
  REG_R13 = 13,
#define REG_R13	REG_R13
  REG_R14 = 14,
#define REG_R14	REG_R14
  REG_R15 = 15
#define REG_R15	REG_R15
};

struct _libc_fpstate
{
  struct
  {
    unsigned int sign1:1;
    unsigned int unused:15;
    unsigned int sign2:1;
    unsigned int exponent:14;
    unsigned int j:1;
    unsigned int mantissa1:31;
    unsigned int mantissa0:32;
  } fpregs[8];
  unsigned int fpsr:32;
  unsigned int fpcr:32;
  unsigned char ftype[8];
  unsigned int init_flag;
};
/* Structure to describe FPU registers.  */
typedef struct _libc_fpstate fpregset_t;

/* Context to describe whole processor state.  This only describes
   the core registers; coprocessor registers get saved elsewhere
   (e.g. in uc_regspace, or somewhere unspecified on the stack
   during non-RT signal handlers).  */
typedef struct sigcontext mcontext_t;

/* Userlevel context.  */
typedef struct ucontext
  {
    unsigned long uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    __sigset_t uc_sigmask;
    unsigned long uc_regspace[128] __attribute__((__aligned__(8)));
  } ucontext_t;

#endif /* sys/ucontext.h */

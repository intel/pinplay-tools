/*******************************************************************************
 * Copyright (c) 2007-2020 Wind River Systems, Inc. and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 * The Eclipse Public License is available at
 * http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 * You may elect to redistribute this code under either of these licenses.
 *
 * Contributors:
 *     Wind River Systems - initial API and implementation
 *******************************************************************************/

#include <tcf/config.h>

#if ENABLE_DebugContext && !ENABLE_ContextProxy

#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <tcf/framework/errors.h>
#include <tcf/framework/cpudefs.h>
#include <tcf/framework/context.h>
#include <tcf/framework/myalloc.h>
#include <tcf/services/symbols.h>
#if ENABLE_ContextMux
#include <tcf/framework/cpudefs-mdep-mux.h>
#endif
#include <tcf/cpudefs-mdep.h>
#include <machine/x86_64/tcf/disassembler-x86_64.h>

#if defined(__i386__) || defined(__x86_64__)

#define REG_OFFSET(name) offsetof(REG_SET, name)
#define REG_SET_SIZE    sizeof(REG_SET)

static RegisterDefinition regs_def[] = {
#if (defined(_WIN32) || defined(__CYGWIN__)) && defined(__i386__)
#   define REG_SP Esp
#   define REG_BP Ebp
#   define REG_IP Eip
    { "eax",    REG_OFFSET(Eax),      4,  0,  0 },
    { "ebx",    REG_OFFSET(Ebx),      4,  3,  3 },
    { "ecx",    REG_OFFSET(Ecx),      4,  1,  1 },
    { "edx",    REG_OFFSET(Edx),      4,  2,  2 },
    { "esp",    REG_OFFSET(Esp),      4,  4,  4 },
    { "ebp",    REG_OFFSET(Ebp),      4,  5,  5 },
    { "esi",    REG_OFFSET(Esi),      4,  6,  6 },
    { "edi",    REG_OFFSET(Edi),      4,  7,  7 },
    { "eip",    REG_OFFSET(Eip),      4,  8,  8 },
    { "eflags", REG_OFFSET(EFlags),   4,  9,  9 },
    { "cs",     REG_OFFSET(SegCs),    2, 51, 51 },
    { "ss",     REG_OFFSET(SegSs),    2, 52, 52 },

    { "ax",     REG_OFFSET(Eax),      2, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 0 },
    { "al",     REG_OFFSET(Eax),      1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 12 },
    { "ah",     REG_OFFSET(Eax) + 1,  1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 12 },

    { "bx",     REG_OFFSET(Ebx),      2, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 1 },
    { "bl",     REG_OFFSET(Ebx),      1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 15 },
    { "bh",     REG_OFFSET(Ebx) + 1,  1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 15 },

    { "cx",     REG_OFFSET(Ecx),      2, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 2 },
    { "cl",     REG_OFFSET(Ecx),      1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 18 },
    { "ch",     REG_OFFSET(Ecx) + 1,  1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 18 },

    { "dx",     REG_OFFSET(Edx),      2, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 3 },
    { "dl",     REG_OFFSET(Edx),      1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 21 },
    { "dh",     REG_OFFSET(Edx) + 1,  1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 21 },

    { "fpu",    0, 0, -1, -1, 0, 0, 1, 1 }, /* 24 */

    { "st0", REG_OFFSET(FloatSave.RegisterArea) + 0,  10, 33, 33, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st1", REG_OFFSET(FloatSave.RegisterArea) + 10, 10, 34, 34, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st2", REG_OFFSET(FloatSave.RegisterArea) + 20, 10, 35, 35, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st3", REG_OFFSET(FloatSave.RegisterArea) + 30, 10, 36, 36, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st4", REG_OFFSET(FloatSave.RegisterArea) + 40, 10, 37, 37, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st5", REG_OFFSET(FloatSave.RegisterArea) + 50, 10, 38, 38, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st6", REG_OFFSET(FloatSave.RegisterArea) + 60, 10, 39, 39, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st7", REG_OFFSET(FloatSave.RegisterArea) + 70, 10, 40, 40, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },

    { "control", REG_OFFSET(FloatSave.ControlWord),  2, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "status",  REG_OFFSET(FloatSave.StatusWord),   2, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "tag",     REG_OFFSET(FloatSave.TagWord),      1, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },

    { "xmm",    0, 0, -1, -1, 0, 0, 1, 1 }, /* 36 */

    { "mxcsr",  REG_OFFSET(ExtendedRegisters) + 24, 4, 64, 64, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 36 },
    { "xmm0",   REG_OFFSET(ExtendedRegisters) + 10 * 16, 16, 17, 17, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 36 },
    { "xmm1",   REG_OFFSET(ExtendedRegisters) + 11 * 16, 16, 18, 18, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 36 },
    { "xmm2",   REG_OFFSET(ExtendedRegisters) + 12 * 16, 16, 19, 19, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 36 },
    { "xmm3",   REG_OFFSET(ExtendedRegisters) + 13 * 16, 16, 20, 20, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 36 },
    { "xmm4",   REG_OFFSET(ExtendedRegisters) + 14 * 16, 16, 21, 21, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 36 },
    { "xmm5",   REG_OFFSET(ExtendedRegisters) + 15 * 16, 16, 22, 22, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 36 },
    { "xmm6",   REG_OFFSET(ExtendedRegisters) + 16 * 16, 16, 23, 23, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 36 },
    { "xmm7",   REG_OFFSET(ExtendedRegisters) + 17 * 16, 16, 24, 24, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 36 },

    { "debug",    0, 0, -1, -1, 0, 0, 1, 1 }, /* 46 */

    { "dr0",    REG_OFFSET(Dr0), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 46 },
    { "dr1",    REG_OFFSET(Dr1), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 46 },
    { "dr2",    REG_OFFSET(Dr2), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 46 },
    { "dr3",    REG_OFFSET(Dr3), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 46 },
    { "dr6",    REG_OFFSET(Dr6), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 46 },
    { "dr7",    REG_OFFSET(Dr7), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 46 },

#elif (defined(_WIN32) || defined(__CYGWIN__)) && defined(__x86_64__)
#   define REG_SP Rsp
#   define REG_BP Rbp
#   define REG_IP Rip
    { "rax",    REG_OFFSET(Rax),      8,  0,  0},
    { "rdx",    REG_OFFSET(Rdx),      8,  1,  1},
    { "rcx",    REG_OFFSET(Rcx),      8,  2,  2},
    { "rbx",    REG_OFFSET(Rbx),      8,  3,  3},
    { "rsi",    REG_OFFSET(Rsi),      8,  4,  4},
    { "rdi",    REG_OFFSET(Rdi),      8,  5,  5},
    { "rbp",    REG_OFFSET(Rbp),      8,  6,  6},
    { "rsp",    REG_OFFSET(Rsp),      8,  7,  7},
    { "r8",     REG_OFFSET(R8),       8,  8,  8},
    { "r9",     REG_OFFSET(R9),       8,  9,  9},
    { "r10",    REG_OFFSET(R10),      8, 10, 10},
    { "r11",    REG_OFFSET(R11),      8, 11, 11},
    { "r12",    REG_OFFSET(R12),      8, 12, 12},
    { "r13",    REG_OFFSET(R13),      8, 13, 13},
    { "r14",    REG_OFFSET(R14),      8, 14, 14},
    { "r15",    REG_OFFSET(R15),      8, 15, 15},
    { "rip",    REG_OFFSET(Rip),      8, 16, 16},
    { "eflags", REG_OFFSET(EFlags),   4, 49, 49},
    { "es",     REG_OFFSET(SegEs),    2, 50, 50},
    { "cs",     REG_OFFSET(SegCs),    2, 51, 51},
    { "ss",     REG_OFFSET(SegSs),    2, 52, 52},
    { "ds",     REG_OFFSET(SegDs),    2, 53, 53},
    { "fs",     REG_OFFSET(SegFs),    2, 54, 54},
    { "gs",     REG_OFFSET(SegGs),    2, 55, 55},

    { "fpu",    0, 0, -1, -1, 0, 0, 1, 1 }, /* 24 */

    { "st0", REG_OFFSET(FltSave.FloatRegisters[0]), 10, 33, 33, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st1", REG_OFFSET(FltSave.FloatRegisters[1]), 10, 34, 34, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st2", REG_OFFSET(FltSave.FloatRegisters[2]), 10, 35, 35, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st3", REG_OFFSET(FltSave.FloatRegisters[3]), 10, 36, 36, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st4", REG_OFFSET(FltSave.FloatRegisters[4]), 10, 37, 37, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st5", REG_OFFSET(FltSave.FloatRegisters[5]), 10, 38, 38, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st6", REG_OFFSET(FltSave.FloatRegisters[6]), 10, 39, 39, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "st7", REG_OFFSET(FltSave.FloatRegisters[7]), 10, 40, 40, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },

    { "control", REG_OFFSET(FltSave.ControlWord),  2, 65, 65, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "status",  REG_OFFSET(FltSave.StatusWord),   2, 66, 66, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },
    { "tag",     REG_OFFSET(FltSave.TagWord),      1, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 24 },

    { "debug",    0, 0, -1, -1, 0, 0, 1, 1 }, /* 36 */

    { "dr0",    REG_OFFSET(Dr0), 8, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 36 },
    { "dr1",    REG_OFFSET(Dr1), 8, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 36 },
    { "dr2",    REG_OFFSET(Dr2), 8, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 36 },
    { "dr3",    REG_OFFSET(Dr3), 8, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 36 },
    { "dr6",    REG_OFFSET(Dr6), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 36 },
    { "dr7",    REG_OFFSET(Dr7), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 36 },

#elif defined(__APPLE__) && defined(__i386__)
#   define REG_SP __esp
#   define REG_BP __ebp
#   define REG_IP __eip
    { "eax",    REG_OFFSET(__eax),    4,  0,  0},
    { "ecx",    REG_OFFSET(__ecx),    4,  1,  1},
    { "edx",    REG_OFFSET(__edx),    4,  2,  2},
    { "ebx",    REG_OFFSET(__ebx),    4,  3,  3},
    { "esp",    REG_OFFSET(__esp),    4,  4,  4},
    { "ebp",    REG_OFFSET(__ebp),    4,  5,  5},
    { "esi",    REG_OFFSET(__esi),    4,  6,  6},
    { "edi",    REG_OFFSET(__edi),    4,  7,  7},
    { "eip",    REG_OFFSET(__eip),    4,  8,  8},
    { "eflags", REG_OFFSET(__eflags), 4,  9,  9},

#elif defined(__APPLE__) && defined(__x86_64__)
#   define REG_SP __rsp
#   define REG_BP __rbp
#   define REG_IP __rip
    { "rax",    REG_OFFSET(__rax),    8,  0,  0},
    { "rdx",    REG_OFFSET(__rdx),    8,  1,  1},
    { "rcx",    REG_OFFSET(__rcx),    8,  2,  2},
    { "rbx",    REG_OFFSET(__rbx),    8,  3,  3},
    { "rsi",    REG_OFFSET(__rsi),    8,  4,  4},
    { "rdi",    REG_OFFSET(__rdi),    8,  5,  5},
    { "rbp",    REG_OFFSET(__rbp),    8,  6,  6},
    { "rsp",    REG_OFFSET(__rsp),    8,  7,  7},
    { "r8",     REG_OFFSET(__r8),     8,  8,  8},
    { "r9",     REG_OFFSET(__r9),     8,  9,  9},
    { "r10",    REG_OFFSET(__r10),    8, 10, 10},
    { "r11",    REG_OFFSET(__r11),    8, 11, 11},
    { "r12",    REG_OFFSET(__r12),    8, 12, 12},
    { "r13",    REG_OFFSET(__r13),    8, 13, 13},
    { "r14",    REG_OFFSET(__r14),    8, 14, 14},
    { "r15",    REG_OFFSET(__r15),    8, 15, 15},
    { "rip",    REG_OFFSET(__rip),    8, 16, 16},
    { "eflags", REG_OFFSET(__rflags), 4, 49, -1},

#elif (defined(__FreeBSD__) || defined(__NetBSD__)) && defined(__i386__)
#   define REG_SP r_esp
#   define REG_BP r_ebp
#   define REG_IP r_eip
    { "eax",    REG_OFFSET(r_eax),    4,  0,  0},
    { "ecx",    REG_OFFSET(r_ecx),    4,  1,  1},
    { "edx",    REG_OFFSET(r_edx),    4,  2,  2},
    { "ebx",    REG_OFFSET(r_ebx),    4,  3,  3},
    { "esp",    REG_OFFSET(r_esp),    4,  4,  4},
    { "ebp",    REG_OFFSET(r_ebp),    4,  5,  5},
    { "esi",    REG_OFFSET(r_esi),    4,  6,  6},
    { "edi",    REG_OFFSET(r_edi),    4,  7,  7},
    { "eip",    REG_OFFSET(r_eip),    4,  8,  8},
    { "eflags", REG_OFFSET(r_eflags), 4,  9,  9},

#elif defined(_WRS_KERNEL) && defined(__i386__)
#   define REG_SP esp
#   define REG_BP ebp
#   define REG_IP eip
    { "eax",    REG_OFFSET(eax),      4,  0,  0},
    { "ebx",    REG_OFFSET(ebx),      4,  3,  3},
    { "ecx",    REG_OFFSET(ecx),      4,  1,  1},
    { "edx",    REG_OFFSET(edx),      4,  2,  2},
    { "esp",    REG_OFFSET(esp),      4,  4,  4},
    { "ebp",    REG_OFFSET(ebp),      4,  5,  5},
    { "esi",    REG_OFFSET(esi),      4,  6,  6},
    { "edi",    REG_OFFSET(edi),      4,  7,  7},
    { "eip",    REG_OFFSET(eip),      4,  8,  8},
    { "eflags", REG_OFFSET(eflags),   4,  9,  9},

    /* FPU */

#define FPU             regs_def + 10
    { "fpu"  , 0,0,-1,-1,0,0,1,1},                      /* FPU folder */

#ifdef _WRS_ARCH_IS_SIMULATOR
   /*
    * 32bits VxSim target
    *
    * See FPO_CONTEXT struct (fppSimlinuxLib.h or fppSimntLib.h)
    */

#define FPU_OFFSET      REG_SET_SIZE
#define FPX_OFFSET      FPU_OFFSET + 0x1c

    { "fpcr" ,FPU_OFFSET +  0x0,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "fpsr" ,FPU_OFFSET +  0x4,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "fptag",FPU_OFFSET +  0x8,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "ip"   ,FPU_OFFSET +  0xc,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "cs"   ,FPU_OFFSET + 0x10,2,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "op"   ,FPU_OFFSET + 0x12,2,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "dp"   ,FPU_OFFSET + 0x14,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "ds"   ,FPU_OFFSET + 0x18,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},

    /* DOUBLEX fpx[FP_NUM_REGS]; */
    { "st/mm0",FPX_OFFSET +  0,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm1",FPX_OFFSET + 10,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm2",FPX_OFFSET + 20,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm3",FPX_OFFSET + 30,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm4",FPX_OFFSET + 40,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm5",FPX_OFFSET + 50,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm6",FPX_OFFSET + 60,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm7",FPX_OFFSET + 70,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
#else
    /*
     * 32bits IA target
     *
     * See FPX_CONTEXT struct  (fppI86Lib.h)
     */

#define FPU_OFFSET      REG_SET_SIZE
#define FPX_OFFSET      FPU_OFFSET+ 0x20

    { "fpcr"  ,FPU_OFFSET +  0x0,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "fpsr"  ,FPU_OFFSET +  0x4,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "fptag" ,FPU_OFFSET +  0x8,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "op"    ,FPU_OFFSET +  0xc,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "ip"    ,FPU_OFFSET + 0x10,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "cs"    ,FPU_OFFSET + 0x14,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "dp"    ,FPU_OFFSET + 0x18,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},
    { "ds"    ,FPU_OFFSET + 0x1c,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0, FPU},

    /* DOUBLEX fpx[FP_NUM_REGS]; */
    { "st/mm0",FPX_OFFSET +  0,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm1",FPX_OFFSET + 10,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm2",FPX_OFFSET + 20,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm3",FPX_OFFSET + 30,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm4",FPX_OFFSET + 40,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm5",FPX_OFFSET + 50,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm6",FPX_OFFSET + 60,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},
    { "st/mm7",FPX_OFFSET + 70,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0, FPU},

    /* XMM */
#define XMM             regs_def + 27
#define XMM_OFFSET      FPX_OFFSET + 80

    /* DOUBLEX_SSE xmm[XMM_NUM_REGS]; */
    { "xmm", 0, 0, -1, -1, 0, 0, 1, 1 },        /* XMM folder */

    { "xmm0", XMM_OFFSET + 0x00,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm1", XMM_OFFSET + 0x10,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm2", XMM_OFFSET + 0x20,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm3", XMM_OFFSET + 0x30,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm4", XMM_OFFSET + 0x40,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm5", XMM_OFFSET + 0x50,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm6", XMM_OFFSET + 0x60,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm7", XMM_OFFSET + 0x70,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
#endif
#elif defined(_WRS_KERNEL) && defined(__x86_64__)
#   define REG_SP rsp
#   define REG_BP rbp
#   define REG_IP rip
    { "rax",    REG_OFFSET(rax),      8,  0,  0},
    { "rdx",    REG_OFFSET(rdx),      8,  1,  1},
    { "rcx",    REG_OFFSET(rcx),      8,  2,  2},
    { "rbx",    REG_OFFSET(rbx),      8,  3,  3},
    { "rsi",    REG_OFFSET(rsi),      8,  4,  4},
    { "rdi",    REG_OFFSET(rdi),      8,  5,  5},
    { "rbp",    REG_OFFSET(rbp),      8,  6,  6},
    { "rsp",    REG_OFFSET(rsp),      8,  7,  7},
    { "r8",     REG_OFFSET(r8),       8,  8,  8},
    { "r9",     REG_OFFSET(r9),       8,  9,  9},
    { "r10",    REG_OFFSET(r10),      8, 10, 10},
    { "r11",    REG_OFFSET(r11),      8, 11, 11},
    { "r12",    REG_OFFSET(r12),      8, 12, 12},
    { "r13",    REG_OFFSET(r13),      8, 13, 13},
    { "r14",    REG_OFFSET(r14),      8, 14, 14},
    { "r15",    REG_OFFSET(r15),      8, 15, 15},
    { "rip",    REG_OFFSET(rip),      8, 16, 16},
    { "eflags", REG_OFFSET(eflags),   4, 49, -1},

    /*
     * 64bit IA & VxSim target
     *
     * See FPREG_SET (fppX86_64Lib.h, fppSimlinuxLib.h or fppSimntLib.h)
     */

    /* FPU */
#define FPU             regs_def + 18
#define FPU_OFFSET      REG_SET_SIZE
#define FPX_OFFSET      FPU_OFFSET+ 0x20

    { "fpu"  , 0,0,-1,-1,0,0,1,1},              /* FPU folder */
    /* FPREG_SET (FPREGX_EXT_SET) */

    { "fpcr" ,FPU_OFFSET +  0,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0,FPU},
    { "fpsr" ,FPU_OFFSET +  4,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0,FPU},
    { "fptag",FPU_OFFSET +  8,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0,FPU},
    { "op"   ,FPU_OFFSET + 12,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0,FPU},
    { "ip"   ,FPU_OFFSET + 16,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0,FPU},
    { "cs"   ,FPU_OFFSET + 20,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0,FPU},
    { "dp"   ,FPU_OFFSET + 24,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0,FPU},
    { "ds"   ,FPU_OFFSET + 28,4,-1,-1,0,0,0,1,0,0,0,0,0,0,0,FPU},

    /* DOUBLEX     fpx[FP_NUM_REGS]; */
    { "st/mm0",FPX_OFFSET +  0,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0,FPU},
    { "st/mm1",FPX_OFFSET + 10,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0,FPU},
    { "st/mm2",FPX_OFFSET + 20,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0,FPU},
    { "st/mm3",FPX_OFFSET + 30,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0,FPU},
    { "st/mm4",FPX_OFFSET + 40,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0,FPU},
    { "st/mm5",FPX_OFFSET + 50,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0,FPU},
    { "st/mm6",FPX_OFFSET + 60,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0,FPU},
    { "st/mm7",FPX_OFFSET + 70,10,-1,-1,0,1,0,1,0,0,0,0,0,0,0,FPU},

    /* XMM */

#define XMM             regs_def + 35
#define XMM_OFFSET      FPX_OFFSET + 80

    /* DOUBLEX_SSE xmm[XMM_NUM_REGS]; */
    { "xmm", 0, 0, -1, -1, 0, 0, 1, 1 },        /* XMM folder */

    { "xmm0", XMM_OFFSET + 0x00,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm1", XMM_OFFSET + 0x10,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm2", XMM_OFFSET + 0x20,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm3", XMM_OFFSET + 0x30,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm4", XMM_OFFSET + 0x40,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm5", XMM_OFFSET + 0x50,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm6", XMM_OFFSET + 0x60,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm7", XMM_OFFSET + 0x70,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm8", XMM_OFFSET + 0x80,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm9", XMM_OFFSET + 0x90,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm10",XMM_OFFSET + 0xa0,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm11",XMM_OFFSET + 0xb0,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm12",XMM_OFFSET + 0xc0,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm13",XMM_OFFSET + 0xd0,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm14",XMM_OFFSET + 0xe0,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},
    { "xmm15",XMM_OFFSET + 0xf0,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, XMM},

    /* YMM */

#define YMM             regs_def + 52
#define YMM_OFFSET      XMM_OFFSET + 0x100
    /* DOUBLEX_SSE ymm[XMM_NUM_REGS]; */
    { "ymm", 0, 0, -1, -1, 0, 0, 1, 1 },        /* YMM folder */

    { "ymm0", YMM_OFFSET + 0x00,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm1", YMM_OFFSET + 0x10,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm2", YMM_OFFSET + 0x20,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm3", YMM_OFFSET + 0x30,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm4", YMM_OFFSET + 0x40,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm5", YMM_OFFSET + 0x50,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm6", YMM_OFFSET + 0x60,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm7", YMM_OFFSET + 0x70,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm8", YMM_OFFSET + 0x80,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm9", YMM_OFFSET + 0x90,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm10",YMM_OFFSET + 0xa0,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm11",YMM_OFFSET + 0xb0,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm12",YMM_OFFSET + 0xc0,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm13",YMM_OFFSET + 0xd0,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm14",YMM_OFFSET + 0xe0,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
    { "ymm15",YMM_OFFSET + 0xf0,16,-1,-1,0,1,0,1,0,0,0,0,0,0,0, YMM},
#elif defined(__x86_64__)
#   define REG_SP user.regs.rsp
#   define REG_BP user.regs.rbp
#   define REG_IP user.regs.rip
    { "rax",    REG_OFFSET(user.regs.rax),      8,  0,  0},
    { "rdx",    REG_OFFSET(user.regs.rdx),      8,  1,  1},
    { "rcx",    REG_OFFSET(user.regs.rcx),      8,  2,  2},
    { "rbx",    REG_OFFSET(user.regs.rbx),      8,  3,  3},
    { "rsi",    REG_OFFSET(user.regs.rsi),      8,  4,  4},
    { "rdi",    REG_OFFSET(user.regs.rdi),      8,  5,  5},
    { "rbp",    REG_OFFSET(user.regs.rbp),      8,  6,  6},
    { "rsp",    REG_OFFSET(user.regs.rsp),      8,  7,  7},
    { "r8",     REG_OFFSET(user.regs.r8),       8,  8,  8},
    { "r9",     REG_OFFSET(user.regs.r9),       8,  9,  9},
    { "r10",    REG_OFFSET(user.regs.r10),      8, 10, 10},
    { "r11",    REG_OFFSET(user.regs.r11),      8, 11, 11},
    { "r12",    REG_OFFSET(user.regs.r12),      8, 12, 12},
    { "r13",    REG_OFFSET(user.regs.r13),      8, 13, 13},
    { "r14",    REG_OFFSET(user.regs.r14),      8, 14, 14},
    { "r15",    REG_OFFSET(user.regs.r15),      8, 15, 15},
    { "rip",    REG_OFFSET(user.regs.rip),      8, 16, 16},
    { "eflags", REG_OFFSET(user.regs.eflags),   4, 49, 49},
    { "es",     REG_OFFSET(user.regs.es),       2, 50, 50},
    { "cs",     REG_OFFSET(user.regs.cs),       2, 51, 51},
    { "ss",     REG_OFFSET(user.regs.ss),       2, 52, 52},
    { "ds",     REG_OFFSET(user.regs.ds),       2, 53, 53},
    { "fs",     REG_OFFSET(user.regs.fs),       2, 54, 54},
    { "gs",     REG_OFFSET(user.regs.gs),       2, 55, 55},
    { "fs_base", REG_OFFSET(user.regs.fs_base), 8, 58, 58},
    { "gs_base", REG_OFFSET(user.regs.gs_base), 8, 59, 59},

    { "fpu",    0, 0, -1, -1, 0, 0, 1, 1 },

    { "st0",    REG_OFFSET(fp.st_space) +   0, 10, 33, 33, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "st1",    REG_OFFSET(fp.st_space) +  16, 10, 34, 34, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "st2",    REG_OFFSET(fp.st_space) +  32, 10, 35, 35, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "st3",    REG_OFFSET(fp.st_space) +  48, 10, 36, 36, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "st4",    REG_OFFSET(fp.st_space) +  64, 10, 37, 37, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "st5",    REG_OFFSET(fp.st_space) +  80, 10, 38, 38, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "st6",    REG_OFFSET(fp.st_space) +  96, 10, 39, 39, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "st7",    REG_OFFSET(fp.st_space) + 112, 10, 40, 40, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },

    { "cwd",    REG_OFFSET(fp.cwd),  2, 65, 65, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "swd",    REG_OFFSET(fp.swd),  2, 66, 66, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "ftw",    REG_OFFSET(fp.ftw),  2, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "fop",    REG_OFFSET(fp.fop),  2, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "fip",    REG_OFFSET(fp.rip),  8, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "fcs",    REG_OFFSET(user.regs.cs),  4, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "foo",    REG_OFFSET(fp.rdp),  8, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "fos",    REG_OFFSET(user.regs.ds),  4, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },

    { "mxcsr",      REG_OFFSET(fp.mxcsr),       4, 64, 64, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },
    { "mxcr_mask",  REG_OFFSET(fp.mxcr_mask),   4, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 26 },

    { "xmm",    0, 0, -1, -1, 0, 0, 1, 1 },

    { "xmm0",   REG_OFFSET(fp.xmm_space) +   0, 16, 17, 17, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm1",   REG_OFFSET(fp.xmm_space) +  16, 16, 18, 18, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm2",   REG_OFFSET(fp.xmm_space) +  32, 16, 19, 19, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm3",   REG_OFFSET(fp.xmm_space) +  48, 16, 20, 20, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm4",   REG_OFFSET(fp.xmm_space) +  64, 16, 21, 21, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm5",   REG_OFFSET(fp.xmm_space) +  80, 16, 22, 22, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm6",   REG_OFFSET(fp.xmm_space) +  96, 16, 23, 23, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm7",   REG_OFFSET(fp.xmm_space) + 112, 16, 24, 24, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm8",   REG_OFFSET(fp.xmm_space) + 128, 16, 25, 25, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm9",   REG_OFFSET(fp.xmm_space) + 144, 16, 26, 26, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm10",  REG_OFFSET(fp.xmm_space) + 160, 16, 27, 27, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm11",  REG_OFFSET(fp.xmm_space) + 176, 16, 28, 28, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm12",  REG_OFFSET(fp.xmm_space) + 192, 16, 29, 29, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm13",  REG_OFFSET(fp.xmm_space) + 208, 16, 30, 30, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm14",  REG_OFFSET(fp.xmm_space) + 224, 16, 31, 31, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },
    { "xmm15",  REG_OFFSET(fp.xmm_space) + 240, 16, 32, 32, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 45 },

    { "debug",    0, 0, -1, -1, 0, 0, 1, 1 }, /* 62 */

    { "dr0",    REG_OFFSET(user.u_debugreg[0]), 8, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 62 },
    { "dr1",    REG_OFFSET(user.u_debugreg[1]), 8, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 62 },
    { "dr2",    REG_OFFSET(user.u_debugreg[2]), 8, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 62 },
    { "dr3",    REG_OFFSET(user.u_debugreg[3]), 8, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 62 },
    { "dr6",    REG_OFFSET(user.u_debugreg[6]), 8, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 62 },
    { "dr7",    REG_OFFSET(user.u_debugreg[7]), 8, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 62 },

    { "fs_base32", REG_OFFSET(other.fs.base), 4, 158, 158, 0, 0, 0, 1 },
    { "gs_base32", REG_OFFSET(other.gs.base), 4, 159, 159, 0, 0, 0, 1 },

#elif defined(__i386__)
#   define REG_SP user.regs.esp
#   define REG_BP user.regs.ebp
#   define REG_IP user.regs.eip
    { "eax",    REG_OFFSET(user.regs.eax),      4,  0,  0 },
    { "ebx",    REG_OFFSET(user.regs.ebx),      4,  3,  3 },
    { "ecx",    REG_OFFSET(user.regs.ecx),      4,  1,  1 },
    { "edx",    REG_OFFSET(user.regs.edx),      4,  2,  2 },
    { "esp",    REG_OFFSET(user.regs.esp),      4,  4,  4 },
    { "ebp",    REG_OFFSET(user.regs.ebp),      4,  5,  5 },
    { "esi",    REG_OFFSET(user.regs.esi),      4,  6,  6 },
    { "edi",    REG_OFFSET(user.regs.edi),      4,  7,  7 },
    { "eip",    REG_OFFSET(user.regs.eip),      4,  8,  8 },
    { "eflags", REG_OFFSET(user.regs.eflags),   4,  9,  9 },
    { "es",     REG_OFFSET(user.regs.xes),      2, 50, 50 },
    { "cs",     REG_OFFSET(user.regs.xcs),      2, 51, 51 },
    { "ss",     REG_OFFSET(user.regs.xss),      2, 52, 52 },
    { "ds",     REG_OFFSET(user.regs.xds),      2, 53, 53 },
    { "fs",     REG_OFFSET(user.regs.xfs),      2, 54, 54 },
    { "gs",     REG_OFFSET(user.regs.xgs),      2, 55, 55 },

    { "ax",     REG_OFFSET(user.regs.eax),      2, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 0 },
    { "al",     REG_OFFSET(user.regs.eax),      1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 16 },
    { "ah",     REG_OFFSET(user.regs.eax) + 1,  1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 16 },

    { "bx",     REG_OFFSET(user.regs.ebx),      2, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 1 },
    { "bl",     REG_OFFSET(user.regs.ebx),      1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 19 },
    { "bh",     REG_OFFSET(user.regs.ebx) + 1,  1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 19 },

    { "cx",     REG_OFFSET(user.regs.ecx),      2, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 2 },
    { "cl",     REG_OFFSET(user.regs.ecx),      1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 22 },
    { "ch",     REG_OFFSET(user.regs.ecx) + 1,  1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 22 },

    { "dx",     REG_OFFSET(user.regs.edx),      2, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 3 },
    { "dl",     REG_OFFSET(user.regs.edx),      1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 25 },
    { "dh",     REG_OFFSET(user.regs.edx) + 1,  1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 25 },

    { "fpu",    0, 0, -1, -1, 0, 0, 1, 1 },

    { "st0",    REG_OFFSET(other.fpx.st_space) +   0, 10, 33, 33, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "st1",    REG_OFFSET(other.fpx.st_space) +  16, 10, 34, 34, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "st2",    REG_OFFSET(other.fpx.st_space) +  32, 10, 35, 35, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "st3",    REG_OFFSET(other.fpx.st_space) +  48, 10, 36, 36, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "st4",    REG_OFFSET(other.fpx.st_space) +  64, 10, 37, 37, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "st5",    REG_OFFSET(other.fpx.st_space) +  80, 10, 38, 38, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "st6",    REG_OFFSET(other.fpx.st_space) +  96, 10, 39, 39, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "st7",    REG_OFFSET(other.fpx.st_space) + 112, 10, 40, 40, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },

    { "cwd",    REG_OFFSET(other.fpx.cwd),  2, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "swd",    REG_OFFSET(other.fpx.swd),  2, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "twd",    REG_OFFSET(other.fpx.twd),  2, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "fop",    REG_OFFSET(other.fpx.fop),  2, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "fip",    REG_OFFSET(other.fpx.fip),  4, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "fcs",    REG_OFFSET(other.fpx.fcs),  4, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "foo",    REG_OFFSET(other.fpx.foo),  4, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },
    { "fos",    REG_OFFSET(other.fpx.fos),  4, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },

    { "mxcsr",  REG_OFFSET(other.fpx.mxcsr), 4, -1, -1, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 28 },

    { "xmm",    0, 0, -1, -1, 0, 0, 1, 1 }, /* 46 */

    { "xmm0",   REG_OFFSET(other.fpx.xmm_space) +   0, 16, 17, 17, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 46 },
    { "xmm1",   REG_OFFSET(other.fpx.xmm_space) +  16, 16, 18, 18, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 46 },
    { "xmm2",   REG_OFFSET(other.fpx.xmm_space) +  32, 16, 19, 19, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 46 },
    { "xmm3",   REG_OFFSET(other.fpx.xmm_space) +  48, 16, 20, 20, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 46 },
    { "xmm4",   REG_OFFSET(other.fpx.xmm_space) +  64, 16, 21, 21, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 46 },
    { "xmm5",   REG_OFFSET(other.fpx.xmm_space) +  80, 16, 22, 22, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 46 },
    { "xmm6",   REG_OFFSET(other.fpx.xmm_space) +  96, 16, 23, 23, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 46 },
    { "xmm7",   REG_OFFSET(other.fpx.xmm_space) + 112, 16, 24, 24, 0, 1, 0,  0,  0,  0,  0,  0,  0,  0,  0, regs_def + 46 },

    { "debug",    0, 0, -1, -1, 0, 0, 1, 1 }, /* 55 */

    { "dr0",    REG_OFFSET(user.u_debugreg[0]), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 55 },
    { "dr1",    REG_OFFSET(user.u_debugreg[1]), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 55 },
    { "dr2",    REG_OFFSET(user.u_debugreg[2]), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 55 },
    { "dr3",    REG_OFFSET(user.u_debugreg[3]), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 55 },
    { "dr6",    REG_OFFSET(user.u_debugreg[6]), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 55 },
    { "dr7",    REG_OFFSET(user.u_debugreg[7]), 4, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, regs_def + 55 },

    { "fs_base", REG_OFFSET(other.fs.base), 4, 58, 58, 0, 0, 0, 1 },
    { "gs_base", REG_OFFSET(other.gs.base), 4, 59, 59, 0, 0, 0, 1 },

#endif

    { NULL },
};

RegisterDefinition * regs_index = NULL;
static unsigned regs_cnt = 0;
static unsigned regs_max = 0;

unsigned char BREAK_INST[] = { 0xcc };

#ifdef MDEP_OtherRegisters

#include <tcf/framework/mdep-ptrace.h>

int mdep_get_other_regs(pid_t pid, REG_SET * data,
                       size_t data_offs, size_t data_size,
                       size_t * done_offs, size_t * done_size) {
    assert(data_offs >= offsetof(REG_SET, other));
    assert(data_offs + data_size <= offsetof(REG_SET, other) + sizeof(data->other));
#if defined(__i386__)
    if (data_offs >= REG_OFFSET(other.fpx) && data_offs < REG_OFFSET(other.fpx) + sizeof(data->other.fpx)) {
        if (ptrace(PTRACE_GETFPXREGS, pid, 0, &data->other.fpx) < 0) return -1;
        *done_offs = offsetof(REG_SET, other.fpx);
        *done_size = sizeof(data->other.fpx);
        return 0;
    }
#endif
    assert(sizeof(data->other.fs) == 16);
    if (data_offs >= REG_OFFSET(other.fs) && data_offs < REG_OFFSET(other.fs) + sizeof(data->other.fs)) {
        if (ptrace(PTRACE_GET_THREAD_AREA, pid, 12, &data->other.fs) < 0) return -1;
        *done_offs = offsetof(REG_SET, other.fs);
        *done_size = sizeof(data->other.fs);
        return 0;
    }
    if (data_offs >= REG_OFFSET(other.gs) && data_offs < REG_OFFSET(other.gs) + sizeof(data->other.gs)) {
        if (ptrace(PTRACE_GET_THREAD_AREA, pid, 13, &data->other.gs) < 0) return -1;
        *done_offs = offsetof(REG_SET, other.gs);
        *done_size = sizeof(data->other.gs);
        return 0;
    }
    errno = ERR_UNSUPPORTED;
    return -1;
}

int mdep_set_other_regs(pid_t pid, REG_SET * data,
                       size_t data_offs, size_t data_size,
                       size_t * done_offs, size_t * done_size) {
    assert(data_offs >= offsetof(REG_SET, other));
    assert(data_offs + data_size <= offsetof(REG_SET, other) + sizeof(data->other));
#if defined(__i386__)
    if (data_offs >= REG_OFFSET(other.fpx) && data_offs < REG_OFFSET(other.fpx) + sizeof(data->other.fpx)) {
        if (ptrace(PTRACE_SETFPXREGS, pid, 0, &data->other.fpx) < 0) return -1;
        *done_offs = offsetof(REG_SET, other.fpx);
        *done_size = sizeof(data->other.fpx);
        return 0;
    }
#endif
    errno = ERR_UNSUPPORTED;
    return -1;
}

#endif

#if defined(__x86_64__)
RegisterDefinition * get_386_reg_by_id(Context * ctx, unsigned id_type, unsigned id) {
    static RegisterIdScope scope;
    switch (id) {
    case 0: /* eax */ id = 0; break;
    case 1: /* ecx */ id = 2; break;
    case 2: /* edx */ id = 1; break;
    case 3: /* ebx */ id = 3; break;
    case 4: /* esp */ id = 7; break;
    case 5: /* ebp */ id = 6; break;
    case 6: /* esi */ id = 4; break;
    case 7: /* edi */ id = 5; break;
    case 8: /* eip */ id = 16; break;
    case 9: /* eflags */ id = 49; break;
    case 58: /* fs_base */ id = 158; break;
    case 59: /* gs_base */ id = 159; break;
    default:
        set_errno(ERR_OTHER, "Invalid register ID");
        return NULL;
    }
    scope.id_type = id_type;
    scope.machine = 62; /* EM_X86_64 */
    return get_reg_by_id(ctx, id, &scope);
}
#endif

#if !ENABLE_ExternalStackcrawl

#ifndef JMPD08
#define JMPD08      0xeb
#endif
#ifndef JMPD32
#define JMPD32      0xe9
#endif
#ifndef PUSH_EBP
#define PUSH_EBP    0x55
#endif
#ifndef ENTER
#define ENTER       0xc8
#endif
#ifndef RET
#define RET         0xc3
#endif
#ifndef RETADD
#define RETADD      0xc2
#endif
#ifndef GRP5
#define GRP5        0xff
#endif
#ifndef JMPN
#define JMPN        0x25
#endif
#ifndef MOVE_mr
#define MOVE_mr     0x89
#endif
#ifndef MOVE_rm
#define MOVE_rm     0x8b
#endif
#ifndef REXW
#define REXW        0x48
#endif
#ifndef POP_EBP
#define POP_EBP     0x5d
#endif

static int read_mem(Context * ctx, ContextAddress address, void * buf, size_t size) {
#if ENABLE_MemoryAccessModes
    static MemoryAccessMode mem_access_mode = { 0, 0, 0, 0, 0, 0, 1 };
    return context_read_mem_ext(ctx, &mem_access_mode, address, buf, size);
#else
    return context_read_mem(ctx, address, buf, size);
#endif
}

static int read_stack(Context * ctx, ContextAddress addr, void * buf, size_t size) {
    if (addr == 0) {
        errno = ERR_INV_ADDRESS;
        return -1;
    }
#ifdef _WRS_KERNEL
    {
        WIND_TCB * tcb = taskTcb(get_context_task_id(ctx));
        if (addr < (ContextAddress)tcb->pStackEnd || addr > (ContextAddress)tcb->pStackBase) {
            errno = ERR_INV_ADDRESS;
            return -1;
        }
    }
#endif
    return read_mem(ctx, addr, buf, size);
}

static int read_reg(StackFrame * frame, RegisterDefinition * def, size_t size, ContextAddress * addr) {
    size_t i;
    uint8_t buf[8];
    uint64_t n = 0;
    *addr = 0;
    assert(!def->big_endian);
    assert(size <= def->size);
    assert(size <= sizeof(buf));
    if (read_reg_bytes(frame, def, 0, size, buf) < 0) return -1;
    for (i = 0; i < size; i++) n |= (uint64_t)buf[i] << (i * 8);
    *addr = (ContextAddress)n;
    return 0;
}

/*
 * trace_jump - resolve any JMP instructions to final destination
 *
 * This routine returns a pointer to the next non-JMP instruction to be
 * executed if the PC were at the specified <adrs>.  That is, if the instruction
 * at <adrs> is not a JMP, then <adrs> is returned.  Otherwise, if the
 * instruction at <adrs> is a JMP, then the destination of the JMP is
 * computed, which then becomes the new <adrs> which is tested as before.
 * Thus we will eventually return the address of the first non-JMP instruction
 * to be executed.
 *
 * The need for this arises because compilers may put JMPs to instructions
 * that we are interested in, instead of the instruction itself.  For example,
 * optimizers may replace a stack pop with a JMP to a stack pop.  Or in very
 * UNoptimized code, the first instruction of a subroutine may be a JMP to
 * a PUSH %EBP MOV %ESP %EBP, instead of a PUSH %EBP MOV %ESP %EBP (compiler
 * may omit routine "post-amble" at end of parsing the routine!).  We call
 * this routine anytime we are looking for a specific kind of instruction,
 * to help handle such cases.
 *
 * RETURNS: The address that a chain of branches points to.
 */
static ContextAddress trace_jump(Context * ctx, ContextAddress addr, int x64, uint32_t * reg_mask) {
    int cnt = 0;
    /* while instruction is a JMP, get destination adrs */
    while (cnt < 100) {
        unsigned char instr;    /* instruction opcode at <addr> */
        ContextAddress dest;    /* Jump destination address */
        if (read_mem(ctx, addr, &instr, 1) < 0) break;

        /* If instruction is a JMP, get destination adrs */
        if (instr == JMPD08) {
            signed char disp08;
            if (read_mem(ctx, addr + 1, &disp08, 1) < 0) break;
            dest = addr + 2 + disp08;
        }
        else if (instr == JMPD32) {
            int32_t disp32 = 0;
            if (read_mem(ctx, addr + 1, &disp32, 4) < 0) break;
            dest = addr + 5 + disp32;
        }
        else if (instr == GRP5) {
            ContextAddress ptr;
            if (read_mem(ctx, addr + 1, &instr, 1) < 0) break;
            if (instr != JMPN) break;
            if (read_mem(ctx, addr + 2, &ptr, sizeof(ptr)) < 0) break;
            if (read_mem(ctx, ptr, &dest, sizeof(dest)) < 0) break;
        }
        else if (instr == MOVE_rm) {
            unsigned char modrm = 0;
            unsigned char mod = 0;
            unsigned char reg = 0;
            unsigned char rm = 0;
            static int reg_to_dwarf_id_32[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
            static int reg_to_dwarf_id_64[] = { 0, 2, 1, 3, 7, 6, 4, 5 };
            int * reg_to_dwarf_id = x64 ? reg_to_dwarf_id_64 : reg_to_dwarf_id_32;
            if (read_mem(ctx, addr + 1, &modrm, 1) < 0) break;
            mod = modrm >> 6;
            reg = (modrm >> 3) & 7u;
            rm = modrm & 7u;
            if (reg == 4 || reg == 5) {
                /* move to SP or BP */
                break;
            }
            if (mod == 0 && (rm == 5 || rm == 6)) {
                break;
            }
            if (mod == 0) {
                dest = addr + 2;
                if (rm == 4) dest++;
            }
            else if (mod == 1) {
                dest = addr + 3;
                if (rm == 4) dest++;
            }
            else if (mod == 2) {
                dest = addr + 6;
                if (rm == 4) dest++;
            }
            else if (mod == 3) {
                dest = addr + 2;
            }
            else {
                break;
            }
            *reg_mask |= (uint32_t)1 << reg_to_dwarf_id[reg];
        }
        else {
            break;
        }
        if (dest == addr) break;
        addr = dest;
        cnt++;
    }
    return addr;
}

static int is_func_entry(unsigned char * code) {
    if (*code != PUSH_EBP) return 0;
    code++;
    if (*code == REXW) code++;
    if (code[0] == MOVE_mr && code[1] == 0xe5) return 1;
    if (code[0] == MOVE_rm && code[1] == 0xec) return 1;
    return 0;
}

static int is_func_exit(unsigned char * code) {
    if (code[0] == POP_EBP && (code[1] == RET || code[1] == RETADD)) return 1;
    if (code[0] == MOVE_rm && code[1] == 0xe5 &&
        code[2] == POP_EBP && (code[3] == RET || code[3] == RETADD)) return 1;
    return 0;
}

int crawl_stack_frame(StackFrame * frame, StackFrame * down) {

    RegisterDefinition * pc_def = NULL;
    RegisterDefinition * sp_def = NULL;
    RegisterDefinition * bp_def = NULL;

    ContextAddress reg_pc = 0;
    ContextAddress reg_bp = 0;

    ContextAddress dwn_pc = 0;
    ContextAddress dwn_sp = 0;
    ContextAddress dwn_bp = 0;

    Context * ctx = frame->ctx;
    size_t word_size = context_word_size(ctx);
    int x64 = word_size == 8;

    {
        RegisterDefinition * r = get_reg_definitions(ctx);
        if (r == NULL) return 0;
        for (; r->name != NULL; r++) {
            if (r->offset == offsetof(REG_SET, REG_IP)) pc_def = r;
            if (r->offset == offsetof(REG_SET, REG_SP)) sp_def = r;
            if (r->offset == offsetof(REG_SET, REG_BP)) bp_def = r;
        }
        if (pc_def == NULL) return 0;
        if (sp_def == NULL) return 0;
        if (bp_def == NULL) return 0;
    }

    if (read_reg(frame, pc_def, word_size, &reg_pc) < 0) return 0;

#if ENABLE_ContextISA
    if (x64) {
        ContextISA isa;
        if (context_get_isa(ctx, reg_pc, &isa) == 0 &&
                isa.isa != NULL && strcmp(isa.isa, "386") == 0) {
            word_size = 4;
            x64 = 0;
        }
    }
#endif

    if (read_reg(frame, bp_def, word_size, &reg_bp) < 0) return 0;

    if (frame->is_top_frame) {
        /* Top frame */
        int copy_regs = 0;
        uint32_t reg_mask = 0;
        ContextAddress reg_sp = 0;
        ContextAddress addr = trace_jump(ctx, reg_pc, x64, &reg_mask);
#if ENABLE_Symbols
        ContextAddress plt = is_plt_section(ctx, addr);
#else
        ContextAddress plt = 0;
#endif
        RegisterDefinition * reg = NULL;

        if (read_reg(frame, sp_def, word_size, &reg_sp) < 0) return 0;
        /*
         * we don't have a stack frame in a few restricted but useful cases:
         *  1) we are at a PUSH %EBP MOV %ESP %EBP or RET or ENTER instruction,
         *  2) we are at the first instruction of a subroutine (this may NOT be
         *     a PUSH %EBP MOV %ESP %EBP instruction with some compilers)
         *  3) we are inside PLT entry
         */
        if (plt) {
            /* TODO: support for large code model PLT */
            if (addr - plt == 0) {
                dwn_sp = reg_sp + word_size * 2;
            }
            else if (addr - plt < 16) {
                dwn_sp = reg_sp + word_size * 3;
            }
            else if ((addr - plt - 16) % 16 < 11) {
                dwn_sp = reg_sp + word_size;
            }
            else {
                dwn_sp = reg_sp + word_size * 2;
            }
            dwn_bp = reg_bp;
            copy_regs = 1;
        }
        else {
            unsigned char code[5];

            if (read_mem(ctx, addr - 1, code, sizeof(code)) < 0) return -1;

            if (code[1] == RET || code[1] == RETADD) {
                dwn_sp = reg_sp + word_size;
                dwn_bp = reg_bp;
                copy_regs = 1;
                reg_mask = 0;
            }
            else if (is_func_entry(code + 1) || code[1] == ENTER) {
                dwn_sp = reg_sp + word_size;
                dwn_bp = reg_bp;
                copy_regs = 1;
            }
            else if (is_func_entry(code)) {
                dwn_sp = reg_sp + word_size * 2;
                dwn_bp = reg_bp;
                copy_regs = 1;
            }
            else if (reg_bp != 0) {
                dwn_sp = reg_bp + word_size * 2;
                if (read_stack(ctx, reg_bp, &dwn_bp, word_size) < 0) dwn_bp = 0;
                if (is_func_exit(code + 1)) {
                    copy_regs = 1;
                    reg_mask = 0;
                }
            }
        }
        if (copy_regs) {
            /* Copy registers that are known to have same value in the down frame */
            for (reg = regs_index; reg->name; reg++) {
                uint8_t buf[16];
                if (reg->dwarf_id < 0) continue;
                if (reg->size == 0 || reg->size > sizeof(buf)) continue;
                if (reg == pc_def || reg == sp_def || reg == bp_def) continue;
                if (reg->dwarf_id < 32 && (reg_mask & ((uint32_t)1 << reg->dwarf_id))) continue;
                if (context_read_reg(ctx, reg, 0, reg->size, buf) < 0) continue;
                if (write_reg_bytes(down, reg, 0, reg->size, buf) < 0) return -1;
            }
        }
    }
    else {
        if (read_stack(ctx, reg_bp, &dwn_bp, word_size) < 0) dwn_bp = 0;
        else dwn_sp = reg_bp + word_size * 2;
    }

    if (read_stack(ctx, dwn_sp - word_size, &dwn_pc, word_size) < 0) dwn_pc = 0;

    if (dwn_bp < reg_bp) dwn_bp = 0;

    if (dwn_pc != 0) {
        if (write_reg_value(down, pc_def, dwn_pc) < 0) return -1;
        if (dwn_sp != 0 && write_reg_value(down, sp_def, dwn_sp) < 0) return -1;
        if (dwn_bp != 0 && write_reg_value(down, bp_def, dwn_bp) < 0) return -1;
        frame->fp = dwn_sp;
    }

    return 0;
}

#endif  /* !ENABLE_ExternalStackcrawl */

RegisterDefinition * get_PC_definition(Context * ctx) {
    static RegisterDefinition * reg_def = NULL;
    if (!context_has_state(ctx)) return NULL;
    if (reg_def == NULL) {
        RegisterDefinition * defs = get_reg_definitions(ctx);
        if (defs != NULL) {
            RegisterDefinition * r;
            for (r = defs; r->name != NULL; r++) {
                if (r->offset == offsetof(REG_SET, REG_IP)) {
                    reg_def = r;
                    break;
                }
            }
        }
    }
    return reg_def;
}

static void ini_main_regs(void) {
    RegisterDefinition * def = regs_def;
    while (def->name != NULL) {
        RegisterDefinition * reg = regs_index + regs_cnt++;
        assert(regs_cnt < regs_max);
        *reg = *def; /* keep references to regs_def array (name, role), as it is a static array */
        if (def->parent != NULL) reg->parent = regs_index + (def->parent - regs_def);
        def++;
    }
}

static void ini_other_regs(void) {
    static int sub_sizes[] = {8, 16, 32, 64, -1};
    char sub_name[256];
    int ix = 0, sub_sizes_ix = 0;

    while (regs_index[ix].name != NULL) {
        if ((strcmp(regs_index[ix].name, "xmm") != 0) &&
            (strcmp(regs_index[ix].name, "ymm") != 0) &&
            (strcmp(regs_index[ix].name, "zmm") != 0) &&
            ((strncmp(regs_index[ix].name, "xmm", 3) == 0) ||
             (strncmp(regs_index[ix].name, "ymm", 3) == 0) ||
             (strncmp(regs_index[ix].name, "zmm", 3) == 0) ||
             (strncmp(regs_index[ix].name, "st/mm", 5) == 0))) {
            for (sub_sizes_ix = 0; sub_sizes[sub_sizes_ix] != -1; sub_sizes_ix++) {
                int sub_ix = 0;
                int nb_sub = (regs_index[ix].size*8) / sub_sizes[sub_sizes_ix];
                RegisterDefinition * def = regs_index + regs_cnt++;
                sprintf(sub_name, "w%d", sub_sizes[sub_sizes_ix]);
                assert(regs_cnt < regs_max);
                def->name = loc_strdup(sub_name);
                def->dwarf_id = -1;
                def->eh_frame_id = -1;
                def->no_read = 1;
                def->no_write = 1;
                def->parent = &regs_index[ix];
                for (sub_ix = 0; sub_ix < nb_sub; sub_ix++) {
                    def = regs_index + regs_cnt++;
                    sprintf(sub_name, "f%d", sub_ix);
                    assert(regs_cnt < regs_max);
                    def->name = loc_strdup(sub_name);
                    def->size = sub_sizes[sub_sizes_ix] / 8;
                    def->offset = regs_index[ix].offset + sub_ix * def->size;
                    def->dwarf_id = -1;
                    def->eh_frame_id = -1;
                    def->big_endian = regs_index[ix].big_endian;
                    def->fp_value = regs_index[ix].fp_value;
                    def->no_read = regs_index[ix].no_read;
                    def->no_write = regs_index[ix].no_write;
                    def->read_once = regs_index[ix].read_once;
                    def->write_once = regs_index[ix].write_once;
                    def->side_effects = regs_index[ix].side_effects;
                    def->volatile_value = regs_index[ix].volatile_value;
                    def->left_to_right = regs_index[ix].left_to_right;
                    def->first_bit = regs_index[ix].first_bit;
                    def->parent = def - sub_ix - 1;
                }
            }
        }

        ix++;
    }
}

static void add_bit_field(RegisterDefinition * parent,
        unsigned pos, unsigned len, const char * name, const char * desc) {
    unsigned i = 0;
    RegisterDefinition * def = regs_index + regs_cnt++;
    assert(regs_cnt < regs_max);
    def->name = name;
    def->dwarf_id = -1;
    def->eh_frame_id = -1;
    def->bits = (int *)loc_alloc_zero(sizeof(int) * (len + 1));
    def->parent = parent;
    def->description = desc;
    while (i < len) def->bits[i++] = pos++;
    def->bits[i++] = -1;
}

static void ini_eflags_bits(void) {
    RegisterDefinition * eflags = regs_index;
    while (eflags->name != NULL && strcmp(eflags->name, "eflags") != 0) eflags++;
    if (eflags->name == NULL) return;
    add_bit_field(eflags,  0, 1, "cf", "Carry Flag");
    add_bit_field(eflags,  2, 1, "pf", "Parity Flag");
    add_bit_field(eflags,  4, 1, "af", "Auxilary Carry Flag");
    add_bit_field(eflags,  6, 1, "zf", "Zero Flag");
    add_bit_field(eflags,  7, 1, "sf", "Sign Flag");
    add_bit_field(eflags,  8, 1, "tf", "Trap Flag");
    add_bit_field(eflags,  9, 1, "if", "Interrupt Enable Flag");
    add_bit_field(eflags, 10, 1, "df", "Direction Flag");
    add_bit_field(eflags, 11, 1, "of", "Overflow Flag");
    add_bit_field(eflags, 12, 2, "iopl", "I/O Privilege Level");
    add_bit_field(eflags, 14, 1, "nt", "Nested Task");
    add_bit_field(eflags, 16, 1, "rf", "Resume Flag");
    add_bit_field(eflags, 17, 1, "vm", "Virtual 8086 Mode");
    add_bit_field(eflags, 18, 1, "ac", "Alignment Check");
    add_bit_field(eflags, 19, 1, "vif", "Virtual Interrupt Flag");
    add_bit_field(eflags, 20, 1, "vip", "Virtual Interrupt Pending");
    add_bit_field(eflags, 21, 1, "id", "ID Flag");
}

#if ENABLE_HardwareBreakpoints

#define MAX_HW_BPS 4

#ifndef ENABLE_BP_ACCESS_INSTRUCTION
#define ENABLE_BP_ACCESS_INSTRUCTION 0
#endif

typedef struct ContextExtensionX86 {
    ContextBreakpoint * triggered_hw_bps[MAX_HW_BPS + 1];
    unsigned            hw_bps_regs_generation;

    ContextBreakpoint * hw_bps[MAX_HW_BPS];
    unsigned            hw_idx[MAX_HW_BPS];
    unsigned            hw_bps_generation;
} ContextExtensionX86;

static size_t context_extension_offset = 0;

#define EXT(ctx) ((ContextExtensionX86 *)((char *)(ctx) + context_extension_offset))

static RegisterDefinition * get_DR_definition(unsigned no) {
    static RegisterDefinition * dr_defs[8];
    if (no > 7) return NULL;
    if (dr_defs[no] == NULL) {
        RegisterDefinition * def = regs_index;
        while (def->name) {
            if (def->name[0] == 'd' && def->name[1] == 'r' &&
                def->name[2] == '0' + (int)no && def->name[3] == 0) {
                dr_defs[no] = def;
                break;
            }
            def++;
        }
    }
    return dr_defs[no];
}

static int skip_read_only_breakpoint(Context * ctx, uint8_t dr6, ContextBreakpoint * bp) {
    unsigned i;
    int read_write_hit = 0;
    ContextExtensionX86 * bps = EXT(context_get_group(ctx, CONTEXT_GROUP_BREAKPOINT));

    for (i = 0; i < MAX_HW_BPS; i++) {
        if (bps->hw_bps[i] != bp) continue;
        if ((dr6 & (1 << i)) == 0) continue;
        if (bps->hw_idx[i] == 0) return 1;
        read_write_hit = 1;
    }
    if (!read_write_hit) return 1;
    if (ctx->stopped_by_cb != NULL) {
        ContextBreakpoint ** p = ctx->stopped_by_cb;
        while (*p != NULL) if (*p++ == bp) return 1;
    }
    return 0;
}

static int set_debug_regs(Context * ctx, int check_ip, int * step_over_hw_bp) {
    unsigned i;
    uint32_t dr7 = 0;
    ContextAddress ip = 0;
    ContextExtensionX86 * ext = EXT(ctx);
    ContextExtensionX86 * bps = EXT(context_get_group(ctx, CONTEXT_GROUP_BREAKPOINT));

    if (check_ip) {
        *step_over_hw_bp = 0;
        if (context_read_reg(ctx, get_PC_definition(ctx), 0, sizeof(ip), &ip) < 0) return -1;
    }

    for (i = 0; i < MAX_HW_BPS; i++) {
        ContextBreakpoint * bp = bps->hw_bps[i];
        if (bp == NULL) {
            /* nothing */
        }
        else if (check_ip && bp->address == ip && (bp->access_types & CTX_BP_ACCESS_INSTRUCTION)) {
            /* Skipping the breakpoint */
            *step_over_hw_bp = 1;
        }
        else {
            if (context_write_reg(ctx, get_DR_definition(i), 0, sizeof(bp->address), &bp->address) < 0) return -1;
            dr7 |= (uint32_t)1 << (i * 2);
            if (bp->access_types == (CTX_BP_ACCESS_INSTRUCTION | CTX_BP_ACCESS_VIRTUAL)) {
                /* nothing */
            }
            else if (bp->access_types == (CTX_BP_ACCESS_DATA_READ | CTX_BP_ACCESS_VIRTUAL)) {
                if (bps->hw_idx[i] == 0) {
                    dr7 |= (uint32_t)1 << (i * 4 + 16);
                }
                else {
                    dr7 |= (uint32_t)3 << (i * 4 + 16);
                }
                dr7 |= 0x100u;
            }
            else if (bp->access_types == (CTX_BP_ACCESS_DATA_WRITE | CTX_BP_ACCESS_VIRTUAL)) {
                dr7 |= (uint32_t)1 << (i * 4 + 16);
                dr7 |= 0x100u;
            }
            else if (bp->access_types == (CTX_BP_ACCESS_DATA_READ | CTX_BP_ACCESS_DATA_WRITE | CTX_BP_ACCESS_VIRTUAL)) {
                dr7 |= (uint32_t)3 << (i * 4 + 16);
                dr7 |= 0x100u;
            }
            else {
                set_errno(ERR_UNSUPPORTED, "Invalid hardware breakpoint: unsupported access mode");
                return -1;
            }
            if (bp->length == 1) {
                /* nothing */
            }
            else if (bp->length == 2) {
                dr7 |= (uint32_t)1 << (i * 4 + 18);
            }
            else if (bp->length == 4) {
                dr7 |= (uint32_t)3 << (i * 4 + 18);
            }
            else if (bp->length == 8) {
                dr7 |= (uint32_t)2 << (i * 4 + 18);
            }
            else {
                set_errno(ERR_UNSUPPORTED, "Invalid hardware breakpoint: unsupported length");
                return -1;
            }
        }
    }
    if (context_write_reg(ctx, get_DR_definition(7), 0, sizeof(dr7), &dr7) < 0) return -1;
    ext->hw_bps_regs_generation = bps->hw_bps_generation;
    if (check_ip && *step_over_hw_bp) ext->hw_bps_regs_generation--;
    return 0;
}

int cpu_bp_get_capabilities(Context * ctx) {
    if (get_DR_definition(0) == NULL) return 0;
    if (ctx != context_get_group(ctx, CONTEXT_GROUP_BREAKPOINT)) return 0;
    return
        CTX_BP_ACCESS_DATA_READ |
        CTX_BP_ACCESS_DATA_WRITE |
#if ENABLE_BP_ACCESS_INSTRUCTION
        CTX_BP_ACCESS_INSTRUCTION |
#endif
        CTX_BP_ACCESS_VIRTUAL;
}

int cpu_bp_plant(ContextBreakpoint * bp) {
    Context * ctx = bp->ctx;
    assert(bp->access_types);
    assert(bp->id == 0);
    if (bp->access_types & CTX_BP_ACCESS_VIRTUAL) {
        ContextExtensionX86 * bps = EXT(ctx);
        if (bp->length <= 8 && ((1u << bp->length) & 0x116u)) {
            unsigned i;
            unsigned n = 1;
            unsigned m = 0;
            if (bp->access_types == (CTX_BP_ACCESS_INSTRUCTION | CTX_BP_ACCESS_VIRTUAL)) {
#if ENABLE_BP_ACCESS_INSTRUCTION
                /* Don't use more then 2 HW slots for instruction access breakpoints */
                int cnt = 0;
                for (i = 0; i < MAX_HW_BPS; i++) {
                    assert(bps->hw_bps[i] != bp);
                    if (bps->hw_bps[i] == NULL) continue;
                    if ((bps->hw_bps[i]->access_types & CTX_BP_ACCESS_INSTRUCTION) == 0) continue;
                    cnt++;
                }
                if (cnt >= MAX_HW_BPS / 2) {
                    errno = ERR_UNSUPPORTED;
                    return -1;
                }
#else
                errno = ERR_UNSUPPORTED;
                return -1;
#endif
            }
            else if (bp->access_types == (CTX_BP_ACCESS_DATA_READ | CTX_BP_ACCESS_VIRTUAL)) {
                n = 2;
            }
            else if (bp->access_types != (CTX_BP_ACCESS_DATA_WRITE | CTX_BP_ACCESS_VIRTUAL) &&
                        bp->access_types != (CTX_BP_ACCESS_DATA_READ | CTX_BP_ACCESS_DATA_WRITE | CTX_BP_ACCESS_VIRTUAL)) {
                errno = ERR_UNSUPPORTED;
                return -1;
            }
            for (i = 0; i < MAX_HW_BPS && m < n; i++) {
                assert(bps->hw_bps[i] != bp);
                if (bps->hw_bps[i] == NULL) {
                    bps->hw_bps[i] = bp;
                    bps->hw_idx[i] = m++;
                    bp->id |= 1 << i;
                }
            }
            if (m == n) {
                LINK * l = context_root.next;
                bps->hw_bps_generation++;
                while (l != &context_root) {
                    Context * c = ctxl2ctxp(l);
                    if (c->stopped && context_get_group(c, CONTEXT_GROUP_BREAKPOINT) == ctx) {
                        if (set_debug_regs(c, 0, NULL) < 0) {
                            for (i = 0; i < MAX_HW_BPS && n > 0; i++) {
                                if (bps->hw_bps[i] == bp) bps->hw_bps[i] = NULL;
                            }
                            bp->id = 0;
                            return -1;
                        }
                    }
                    l = l->next;
                }
                return 0;
            }
            for (i = 0; i < MAX_HW_BPS && n > 0; i++) {
                if (bps->hw_bps[i] == bp) bps->hw_bps[i] = NULL;
            }
            set_errno(ERR_UNSUPPORTED, "All hardware breakpoints are already in use");
            bp->id = 0;
            return -1;
        }
    }
    errno = ERR_UNSUPPORTED;
    return -1;
}

int cpu_bp_remove(ContextBreakpoint * bp) {
    unsigned i;
    LINK * l = NULL;
    Context * ctx = bp->ctx;
    ContextExtensionX86 * bps = EXT(ctx);
    assert(bp->id != 0);
    for (i = 0; i < MAX_HW_BPS; i++) {
        if (bps->hw_bps[i] == bp) {
            bps->hw_bps[i] = NULL;
            bps->hw_bps_generation++;
            assert(bp->id & (1 << i));
        }
    }
    l = context_root.next;
    while (l != &context_root) {
        Context * c = ctxl2ctxp(l);
        if (c->stopped && context_get_group(c, CONTEXT_GROUP_BREAKPOINT) == ctx) {
            if (set_debug_regs(c, 0, NULL) < 0) return -1;
        }
        l = l->next;
    }
    bp->id = 0;
    return 0;
}

int cpu_bp_on_resume(Context * ctx, int * single_step) {
    /* Update debug registers */
    ContextExtensionX86 * ext = EXT(ctx);
    ContextExtensionX86 * bps = EXT(context_get_group(ctx, CONTEXT_GROUP_BREAKPOINT));
    if (ctx->stopped_by_cb != NULL || ext->hw_bps_regs_generation != bps->hw_bps_generation) {
        if (set_debug_regs(ctx, 1, single_step) < 0) return -1;
    }
    return 0;
}

int cpu_bp_on_suspend(Context * ctx, int * triggered) {
    unsigned cb_cnt = 0;
    uint8_t dr6 = 0;

    if (ctx->exiting) return 0;
    if (context_read_reg(ctx, get_DR_definition(6), 0, sizeof(dr6), &dr6) < 0) return -1;

    if (dr6 & 0xfu) {
        unsigned i;
        ContextExtensionX86 * ext = EXT(ctx);
        ContextExtensionX86 * bps = EXT(context_get_group(ctx, CONTEXT_GROUP_BREAKPOINT));
        for (i = 0; i < MAX_HW_BPS; i++) {
            if (dr6 & ((uint32_t)1 << i)) {
                ContextBreakpoint * bp = bps->hw_bps[i];
                if (bp == NULL) continue;
                assert(bp->id != 0);
                if (bp->access_types == (CTX_BP_ACCESS_DATA_READ | CTX_BP_ACCESS_VIRTUAL)) {
                    if (skip_read_only_breakpoint(ctx, dr6, bp)) continue;
                }
                ext->triggered_hw_bps[cb_cnt++] = bp;
            }
        }
        dr6 = 0;
        if (context_write_reg(ctx, get_DR_definition(6), 0, sizeof(dr6), &dr6) < 0) return -1;
        if (cb_cnt > 0) {
            ctx->stopped_by_cb = ext->triggered_hw_bps;
            ctx->stopped_by_cb[cb_cnt] = NULL;
        }
    }
    *triggered = cb_cnt > 0;
    return 0;
}

#endif /* ENABLE_HardwareBreakpoints */

#if defined(ENABLE_add_cpudefs_disassembler) && ENABLE_add_cpudefs_disassembler
void add_cpudefs_disassembler(Context * cpu_ctx) {
    add_disassembler(cpu_ctx, "386", disassemble_x86_32);
    add_disassembler(cpu_ctx, "X86_64", disassemble_x86_64);
}
#endif

void ini_cpudefs_mdep(void) {
    regs_cnt = 0;
    regs_max = 2000;
    regs_index = (RegisterDefinition *)loc_alloc_zero(sizeof(RegisterDefinition) * regs_max);
    ini_main_regs();
    ini_eflags_bits();
    ini_other_regs();
#if ENABLE_HardwareBreakpoints
    context_extension_offset = context_extension(sizeof(ContextExtensionX86));
#endif
}

#endif
#endif

/*******************************************************************************
 * Copyright (c) 2012-2019 Wind River Systems, Inc. and others.
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

#if defined(__linux__)

#include <elf.h>
#include <tcf/framework/mdep-ptrace.h>

/* additional CPU registers */

#if !defined(PTRACE_GETFPXREGS) && !defined(PT_GETFPXREGS)
#define PTRACE_GETFPXREGS 18
#endif

#if !defined(PTRACE_SETFPXREGS) && !defined(PT_SETFPXREGS)
#define PTRACE_SETFPXREGS 19
#endif

#ifndef PTRACE_GET_THREAD_AREA
#define PTRACE_GET_THREAD_AREA 25
#endif

struct i386_user_desc {
    uint32_t entry;
    uint32_t base;
    uint32_t limit;
    uint32_t status;
};

struct i386_regset_extra {
    struct user_fpxregs_struct fpx;
    struct i386_user_desc fs;
    struct i386_user_desc gs;
};

#define MDEP_OtherRegisters struct i386_regset_extra

#endif

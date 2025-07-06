/*******************************************************************************
 * Copyright (c) 2009-2017 Wind River Systems, Inc. and others.
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
 *     Stanislav Yakovlev - [404627] Add support for ARM VFP registers
 *******************************************************************************/

/*
 * This header file provides definition of REG_SET - a structure that can
 * hold values of target CPU registers.
 */

#if defined(__linux__)

#include <sys/user.h>
#include <tcf/regset-mdep.h>

#if defined(__powerpc__)
struct user_fpregs_struct {
    uint64_t fpregs[32];
    uint32_t fpscr;
};
#endif

typedef struct REG_SET {
#if !defined(MDEP_UseREGSET)
    struct user user;
    struct user_fpregs_struct fp;
#else
    /* Since Linux 2.6.34 */
    struct regset_gp gp; /* General purpose registers */
    struct regset_fp fp; /* Floating point registers */
#endif

/* Additional CPU registers - defined in regset-mdep.h */
#ifdef MDEP_OtherRegisters
    MDEP_OtherRegisters other;
#endif
} REG_SET;

#ifdef MDEP_OtherRegisters

/*
 * Get/set additional CPU registers.
 * Implementation is architecture specific.
 * data_offs and data_size - offset and size of portion of 'data' to get/set.
 * done_offs and done_size - offset and size of actually trasfered data.
 * Actual transfer should, at least, include requested part of data.
 * On success return 0 and set done_offs/done_size.
 * On error return -1 and set errno.
 */

extern int mdep_get_other_regs(pid_t pid, REG_SET * data,
                               size_t data_offs, size_t data_size,
                               size_t * done_offs, size_t * done_size);

extern int mdep_set_other_regs(pid_t pid, REG_SET * data,
                               size_t data_offs, size_t data_size,
                               size_t * done_offs, size_t * done_size);
#endif

#endif

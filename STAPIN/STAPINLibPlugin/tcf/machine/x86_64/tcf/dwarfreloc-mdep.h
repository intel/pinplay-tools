/*******************************************************************************
 * Copyright (c) 2010-2021 Wind River Systems, Inc. and others.
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

/*
 * This module provides CPU specific ELF definitions for X86.
 */

#define R_X86_64_NONE  0
#define R_X86_64_64    1
#define R_X86_64_PC32  2
#define R_X86_64_32    10
#define R_X86_64_32S   11
#define R_X86_64_PC64  24

static void elf_relocate(void) {
    if (relocs->type == SHT_REL && reloc_type != R_X86_64_NONE) {
        if (section->file->type != ET_REL) str_exception(ERR_INV_FORMAT, "Invalid relocation record");
        assert(reloc_addend == 0);
        switch (reloc_type) {
        case R_X86_64_64:
        case R_X86_64_PC64:
            {
                U8_T x = *(U8_T *)((char *)section->data + reloc_offset);
                if (section->file->byte_swap) SWAP(x);
                reloc_addend = x;
            }
            break;
        default:
            {
                U4_T x = *(U4_T *)((char *)section->data + reloc_offset);
                if (section->file->byte_swap) SWAP(x);
                reloc_addend = x;
            }
            break;
        }
    }
    switch (reloc_type) {
    case R_X86_64_NONE:
        *destination_section = NULL;
        break;
    case R_X86_64_32:
    case R_X86_64_32S:
        if (data_size < 4) str_exception(ERR_INV_FORMAT, "Invalid relocation record");
        *(U4_T *)data_buf = (U4_T)(sym_value + reloc_addend);
        break;
    case R_X86_64_64:
        if (data_size < 8) str_exception(ERR_INV_FORMAT, "Invalid relocation record");
        *(U8_T *)data_buf = (U8_T)(sym_value + reloc_addend);
        break;
    case R_X86_64_PC32:
        if (data_size < 4) str_exception(ERR_INV_FORMAT, "Invalid relocation record");
        *(U4_T *)data_buf = (U4_T)(sym_value + reloc_addend - (section->addr + reloc_offset));
        break;
    case R_X86_64_PC64:
        if (data_size < 8) str_exception(ERR_INV_FORMAT, "Invalid relocation record");
        *(U8_T *)data_buf = (U8_T)(sym_value + reloc_addend - (section->addr + reloc_offset));
        break;
    default:
        str_fmt_exception(ERR_INV_FORMAT, "Unsupported relocation type %u", (unsigned)reloc_type);
    }
}

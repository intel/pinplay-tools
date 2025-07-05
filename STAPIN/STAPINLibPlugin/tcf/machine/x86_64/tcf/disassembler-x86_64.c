/*******************************************************************************
 * Copyright (c) 2015-2022 Xilinx, Inc. and others.
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
 *     Xilinx - initial API and implementation
 *******************************************************************************/

#include <tcf/config.h>

#if SERVICE_Disassembly

#include <assert.h>
#include <stdio.h>
#include <tcf/framework/context.h>
#include <tcf/services/symbols.h>
#include <machine/x86_64/tcf/disassembler-x86_64.h>

#define MODE_16     0
#define MODE_32     1
#define MODE_64     2

#define PREFIX_LOCK         0x0001
#define PREFIX_REPNZ        0x0002 /* 0xf2 */
#define PREFIX_REPZ         0x0004 /* 0xf3 */
#define PREFIX_CS           0x0008 /* Also "Branch not taken" hint */
#define PREFIX_SS           0x0010
#define PREFIX_DS           0x0020 /* Also "Branch taken" hint */
#define PREFIX_ES           0x0040
#define PREFIX_FS           0x0080
#define PREFIX_GS           0x0100
#define PREFIX_DATA_SIZE    0x0200
#define PREFIX_ADDR_SIZE    0x0400
#define PREFIX_FWAIT        0x0800 /* 0x9b */

#define REX_W               0x08
#define REX_R               0x04
#define REX_X               0x02
#define REX_B               0x01

#define REG_SIZE_ST         10
#define REG_SIZE_MMX        11
#define REG_SIZE_XMM        12
#define REG_SIZE_YMM        13
#define REG_SIZE_SEG        14
#define REG_SIZE_CR         15
#define REG_SIZE_DR         16

#define op_size(modrm) (((modrm >> 6) & 3) == 3 ? 0 : data_size)

static char buf[128];
static size_t buf_pos = 0;
static DisassemblerParams * params = NULL;
static uint64_t instr_addr = 0;
static uint8_t * code_buf = NULL;
static size_t code_pos = 0;
static size_t code_len = 0;
static uint32_t prefix = 0;
static uint32_t vex = 0;
static uint8_t rex = 0;
static unsigned data_size = 0;
static unsigned push_size = 0;
static unsigned addr_size = 0;
static int x86_mode = 0;
static uint8_t modrm = 0;
static uint8_t modrm_sib = 0;
static uint32_t modrm_offs = 0;

static uint8_t get_code(void) {
    uint8_t c = 0;
    if (code_pos < code_len) c = code_buf[code_pos];
    code_pos++;
    return c;
}

static uint32_t get_disp8(void) {
    uint32_t disp = get_code();
    if (disp >= 0x80) {
        disp |= 0xffffff00;
    }
    return disp;
}

static uint32_t get_disp16(void) {
    uint32_t disp = get_code();
    disp |= (uint32_t)get_code() << 8;
    return disp;
}

static uint32_t get_disp32(void) {
    uint32_t disp = get_code();
    disp |= (uint32_t)get_code() << 8;
    disp |= (uint32_t)get_code() << 16;
    disp |= (uint32_t)get_code() << 24;
    return disp;
}

static void get_modrm(void) {
    unsigned mod = 0;
    unsigned rm = 0;
    modrm = get_code();
    modrm_sib = 0;
    modrm_offs = 0;
    mod = (modrm >> 6) & 3;
    rm = modrm & 7;

    if (mod == 3) return;
    if (addr_size >= 4 && rm == 4) modrm_sib = get_code();
    switch (mod) {
    case 0:
        if (addr_size == 2 && rm == 6) modrm_offs = get_disp16();
        if (addr_size == 4 && rm == 5) modrm_offs = get_disp32();
        if (addr_size == 8 && rm == 5) modrm_offs = get_disp32();
        break;
    case 1:
        modrm_offs = get_disp8();
        break;
    case 2:
        if (addr_size <= 2) modrm_offs = get_disp16();
        else modrm_offs = get_disp32();
        break;
    }
    if (addr_size >= 4 && rm == 4) {
        unsigned base = modrm_sib & 7;
        if (base == 5 && mod == 0 && rm != 5) {
            modrm_offs = get_disp32();
        }
    }
}

static void add_char(char ch) {
    if (buf_pos >= sizeof(buf) - 1) return;
    buf[buf_pos++] = ch;
    if (ch == ' ') {
        if (buf_pos >= 4 && strncmp(buf, "bnd ", 4) == 0) return;
        if (buf_pos >= 4 && strncmp(buf, "rep ", 4) == 0) return;
        if (buf_pos >= 5 && strncmp(buf, "repz ", 5) == 0) return;
        if (buf_pos >= 6 && strncmp(buf, "repnz ", 6) == 0) return;
        while (buf_pos < 8) buf[buf_pos++] = ch;
    }
}

static void add_str(const char * s) {
    while (*s) add_char(*s++);
}

static void add_cmd(const char * s, unsigned size) {
    while (*s) add_char(*s++);
    switch (size) {
    case 1: add_char('b'); break;
    case 2: add_char('w'); break;
    case 4: add_char('l'); break;
    case 8: add_char('q'); break;
    }
    add_char(' ');
}

static void add_dec_uint32(uint32_t n) {
    char s[32];
    size_t i = 0;
    do {
        s[i++] = (char)('0' + n % 10);
        n = n / 10;
    }
    while (n != 0);
    while (i > 0) add_char(s[--i]);
}

#if 0 /* Not used yet */
static void add_dec_uint64(uint64_t n) {
    char s[64];
    size_t i = 0;
    do {
        s[i++] = (char)('0' + (int)(n % 10));
        n = n / 10;
    }
    while (n != 0);
    while (i > 0) add_char(s[--i]);
}
#endif

static void add_hex_uint32(uint32_t n) {
    char s[32];
    size_t i = 0;
    while (i < 8) {
        uint32_t d = n & 0xf;
        if (i > 0 && n == 0) break;
        s[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        n = n >> 4;
    }
    while (i > 0) add_char(s[--i]);
}

static void add_hex_uint64(uint64_t n) {
    char s[64];
    size_t i = 0;
    while (i < 16) {
        uint32_t d = n & 0xf;
        if (i > 0 && n == 0) break;
        s[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        n = n >> 4;
    }
    while (i > 0) add_char(s[--i]);
}

static void add_0x_hex_uint32(uint32_t n) {
    add_str("0x");
    add_hex_uint32(n);
}

static void add_0x_hex_int32(int32_t n) {
    if (n < 0) {
        add_char('-');
        add_0x_hex_uint32(-n);
    }
    else {
        add_0x_hex_uint32(n);
    }
}

static void add_0x_hex_uint64(uint64_t n) {
    add_str("0x");
    add_hex_uint64(n);
}

#if 0 /* Not used yet */
static void add_flt_uint32(uint32_t n) {
    char buf[32];
    union {
        uint32_t n;
        float f;
    } u;
    u.n = n;
    snprintf(buf, sizeof(buf), "%g", u.f);
    add_str(buf);
}

static void add_flt_uint64(uint64_t n) {
    char buf[32];
    union {
        uint64_t n;
        double d;
    } u;
    u.n = n;
    snprintf(buf, sizeof(buf), "%g", u.d);
    add_str(buf);
}
#endif

static void add_addr(uint64_t addr) {
    add_hex_uint64(addr);
#if ENABLE_Symbols
    if (params->ctx != NULL) {
        Symbol * sym = NULL;
        char * name = NULL;
        ContextAddress sym_addr = 0;
        if (find_symbol_by_addr(params->ctx, STACK_NO_FRAME, (ContextAddress)addr, &sym) < 0) return;
        if (get_symbol_name(sym, &name) < 0 || name == NULL) return;
        if (get_symbol_address(sym, &sym_addr) < 0) return;
        if (sym_addr <= addr) {
            add_str(" <");
            add_str(name);
            if (sym_addr < addr) {
                add_char('+');
                add_0x_hex_uint64(addr - (uint64_t)sym_addr);
            }
            add_str(">");
        }
    }
#endif
}

static void add_reg(unsigned reg, unsigned size) {
    add_char('%');
    switch (size) {
    case REG_SIZE_ST:
        add_str("st");
        add_dec_uint32(reg & 7);
        return;
    case REG_SIZE_MMX:
        add_str("mm");
        add_dec_uint32(reg & 7);
        return;
    case REG_SIZE_XMM:
        add_str("xmm");
        add_dec_uint32(reg);
        return;
    case REG_SIZE_YMM:
        add_str("ymm");
        add_dec_uint32(reg);
        return;
    case REG_SIZE_SEG:
        switch (reg & 7) {
        case 0: add_str("es"); break;
        case 1: add_str("cs"); break;
        case 2: add_str("ss"); break;
        case 3: add_str("ds"); break;
        case 4: add_str("fs"); break;
        case 5: add_str("gs"); break;
        case 6: add_str("s6"); break;
        case 7: add_str("s7"); break;
        }
        return;
    case REG_SIZE_CR:
        add_str("cr");
        add_dec_uint32(reg);
        return;
    case REG_SIZE_DR:
        add_str("dr");
        add_dec_uint32(reg);
        return;
    }
    assert(size <= 8);
    if (reg >= 8) {
        add_char('r');
        add_dec_uint32(reg);
        switch (size) {
        case 1: add_char('b'); break;
        case 2: add_char('w'); break;
        case 4: add_char('d'); break;
        }
        return;
    }
    if (rex && size == 1 && reg >= 4 && reg <= 7) {
        switch (reg) {
        case 4: add_str("spl"); break;
        case 5: add_str("bpl"); break;
        case 6: add_str("sil"); break;
        case 7: add_str("dil"); break;
        }
        return;
    }
    if (size == 1) {
        switch (reg) {
        case 0: add_str("al"); break;
        case 1: add_str("cl"); break;
        case 2: add_str("dl"); break;
        case 3: add_str("bl"); break;
        case 4: add_str("ah"); break;
        case 5: add_str("ch"); break;
        case 6: add_str("dh"); break;
        case 7: add_str("bh"); break;
        }
    }
    else {
        switch (size) {
        case 4: add_char('e'); break;
        case 8: add_char('r'); break;
        }
        switch (reg) {
        case 0: add_str("ax"); break;
        case 1: add_str("cx"); break;
        case 2: add_str("dx"); break;
        case 3: add_str("bx"); break;
        case 4: add_str("sp"); break;
        case 5: add_str("bp"); break;
        case 6: add_str("si"); break;
        case 7: add_str("di"); break;
        }
    }
}

static void add_ttt(unsigned ttt) {
    switch (ttt) {
    case  0: add_str("o"); break;
    case  1: add_str("no"); break;
    case  2: add_str("b"); break;
    case  3: add_str("ae"); break;
    case  4: add_str("e"); break;
    case  5: add_str("ne"); break;
    case  6: add_str("be"); break;
    case  7: add_str("a"); break;
    case  8: add_str("s"); break;
    case  9: add_str("ns"); break;
    case 10: add_str("p"); break;
    case 11: add_str("np"); break;
    case 12: add_str("l"); break;
    case 13: add_str("ge"); break;
    case 14: add_str("le"); break;
    case 15: add_str("g"); break;
    }
}

static void add_imm8(unsigned size) {
    uint32_t imm = get_code();
    add_str("$0x");
    if (imm & 0x80) {
        while (size > 1) {
            add_str("ff");
            size--;
        }
    }
    add_hex_uint32(imm);
}

static void add_imm16(void) {
    uint32_t imm = get_code();
    imm |= (uint32_t)get_code() << 8;
    add_char('$');
    add_0x_hex_uint32(imm);
}

static void add_imm32(void) {
    uint32_t imm = get_code();
    imm |= (uint32_t)get_code() << 8;
    imm |= (uint32_t)get_code() << 16;
    imm |= (uint32_t)get_code() << 24;
    add_char('$');
    add_0x_hex_uint32(imm);
}

static void add_imm32s(unsigned size) {
    uint32_t imm = get_code();
    imm |= (uint32_t)get_code() << 8;
    imm |= (uint32_t)get_code() << 16;
    imm |= (uint32_t)get_code() << 24;
    add_str("$0x");
    if (imm & 0x80000000) {
        while (size > 4) {
            add_str("ff");
            size--;
        }
    }
    add_hex_uint32(imm);
}

static void add_imm64(void) {
    uint64_t imm = get_code();
    imm |= (uint64_t)get_code() << 8;
    imm |= (uint64_t)get_code() << 16;
    imm |= (uint64_t)get_code() << 24;
    imm |= (uint64_t)get_code() << 32;
    imm |= (uint64_t)get_code() << 40;
    imm |= (uint64_t)get_code() << 48;
    imm |= (uint64_t)get_code() << 56;
    add_char('$');
    add_0x_hex_uint64(imm);
}

static void add_moffs(int wide) {
    uint64_t addr = 0;
    unsigned i = 0;

    while (i < addr_size) {
        addr |= (uint64_t)get_code() << (i * 8);
        i++;
    }

    if (prefix & PREFIX_CS) add_str("%cs:");
    if (prefix & PREFIX_SS) add_str("%ss:");
    if (prefix & PREFIX_DS) add_str("%ds:");
    if (prefix & PREFIX_ES) add_str("%es:");
    if (prefix & PREFIX_FS) add_str("%fs:");
    if (prefix & PREFIX_GS) add_str("%gs:");

    add_0x_hex_uint64(addr);
}

static void add_rel(unsigned size) {
    uint64_t offs = 0;
    uint64_t sign = (uint64_t)1 << (size * 8 - 1);
    uint64_t mask = sign - 1;
    unsigned i = 0;

    while (i < size) {
        offs |= (uint64_t)get_code() << (i * 8);
        i++;
    }

    if (offs & sign) {
        offs = (offs ^ (sign | mask)) + 1;
        add_addr(instr_addr + code_pos - offs);
    }
    else {
        add_addr(instr_addr + code_pos + offs);
    }
}

static void add_modrm_reg(unsigned size) {
    unsigned rm = (modrm >> 3) & 7;
    if (rex & REX_R) rm += 8;
    add_reg(rm, size);
}

static void add_modrm_mem(unsigned size) {
    unsigned mod = (modrm >> 6) & 3;
    unsigned rm = modrm & 7;
    if (mod == 3) {
        if (rex & REX_B) rm += 8;
        add_reg(rm, size);
    }
    else {
        if (prefix & PREFIX_CS) add_str("%cs:");
        if (prefix & PREFIX_SS) add_str("%ss:");
        if (prefix & PREFIX_DS) add_str("%ds:");
        if (prefix & PREFIX_ES) add_str("%es:");
        if (prefix & PREFIX_FS) add_str("%fs:");
        if (prefix & PREFIX_GS) add_str("%gs:");
        switch (mod) {
        case 0:
            if (addr_size == 2 && rm == 6) add_0x_hex_int32(modrm_offs);
            if (addr_size == 4 && rm == 5) add_0x_hex_int32(modrm_offs);
            if (addr_size == 8 && rm == 5) add_0x_hex_int32(modrm_offs);
            break;
        case 1:
            add_0x_hex_int32(modrm_offs);
            break;
        case 2:
            add_0x_hex_int32(modrm_offs);
            break;
        }
        if (addr_size < 4) {
            add_char('(');
            switch (rm) {
            case 0: add_str("bx+si"); break;
            case 1: add_str("bx+di"); break;
            case 2: add_str("bp+si"); break;
            case 3: add_str("bp+di"); break;
            case 4: add_str("si"); break;
            case 5: add_str("di"); break;
            case 6: if (mod != 0) add_str("bp"); break;
            case 7: add_str("bx"); break;
            }
            add_char(')');
        }
        else {
            if (rm == 5 && mod == 0) {
                if (x86_mode == MODE_64) {
                    add_char('(');
                    add_str(addr_size > 4 ? "%rip" : "%eip");
                    add_char(')');
                }
            }
            else if (rm == 4) {
                unsigned base = modrm_sib & 7;
                unsigned index = (modrm_sib >> 3) & 7;
                unsigned scale = (modrm_sib >> 6) & 3;
                int bs = 0;
                if (base == 5 && mod == 0) {
                    if (index == 4 && (rex & REX_X) == 0) {
                        /* Absolute address */
                        if (addr_size == 8) add_0x_hex_uint64((int32_t)modrm_offs);
                        else add_0x_hex_uint32(modrm_offs);
                    }
                    else {
                        add_0x_hex_int32(modrm_offs);
                    }
                }
                if (base != 5 || mod == 1 || mod == 2) {
                    add_char('(');
                    if (rex & REX_B) add_reg(base + 8, addr_size);
                    else add_reg(base, addr_size);
                    bs = 1;
                }
                if (index != 4 || (rex & REX_X) != 0) {
                    if (!bs) add_char('(');
                    add_char(',');
                    if (rex & REX_X) add_reg(index + 8, addr_size);
                    else add_reg(index, addr_size);
                    add_char(',');
                    switch (scale) {
                    case 1: add_char('2'); break;
                    case 2: add_char('4'); break;
                    case 3: add_char('8'); break;
                    default: add_char('1'); break;
                    }
                    bs = 1;
                }
                if (bs) add_char(')');
            }
            else {
                add_char('(');
                if (rex & REX_B) rm += 8;
                add_reg(rm, addr_size);
                add_char(')');
            }
        }
    }
}

static void add_modrm(unsigned reg_size, unsigned mem_size, int swap) {
    get_modrm();
    if (swap) {
        add_modrm_mem(mem_size);
        add_char(',');
        add_modrm_reg(reg_size);
    }
    else {
        add_modrm_reg(reg_size);
        add_char(',');
        add_modrm_mem(mem_size);
    }
}

static void add_a_op(unsigned op, unsigned size) {
    switch (op) {
    case 0: add_cmd("add", size); break;
    case 1: add_cmd("or", size); break;
    case 2: add_cmd("adc", size); break;
    case 3: add_cmd("sbb", size); break;
    case 4: add_cmd("and", size); break;
    case 5: add_cmd("sub", size); break;
    case 6: add_cmd("xor", size); break;
    case 7: add_cmd("cmp", size); break;
    }
}

static int shift_op(unsigned op, unsigned size) {
    switch (op) {
    case 0: add_cmd("rol", size); return 1;
    case 1: add_cmd("ror", size); return 1;
    case 2: add_cmd("rcl", size); return 1;
    case 3: add_cmd("rcr", size); return 1;
    case 4: add_cmd("shl", size); return 1;
    case 5: add_cmd("shr", size); return 1;
    case 7: add_cmd("sar", size); return 1;
    }
    return 0;
}

static void disassemble_0f(void) {
    uint8_t opcode = get_code();

    switch (opcode) {
    case 0x08:
        add_str("invd");
        return;
    case 0x09:
        add_str("wbind");
        return;
    case 0x0b:
        add_str("ud2");
        return;
    case 0x10:
    case 0x11:
        add_str("mov");
        if (prefix & PREFIX_REPZ) {
            add_str("ss ");
            add_modrm(REG_SIZE_XMM, 4, (opcode & 1) == 0);
            return;
        }
        if (prefix & PREFIX_REPNZ) {
            add_str("sd ");
            add_modrm(REG_SIZE_XMM, 8, (opcode & 1) == 0);
            return;
        }
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("upd ");
            add_modrm(REG_SIZE_XMM, 16, (opcode & 1) == 0);
            return;
        }
        add_str("ups ");
        add_modrm(REG_SIZE_XMM, 16, (opcode & 1) == 0);
        return;
    case 0x17:
        add_str("mov");
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("hpd ");
            add_modrm(REG_SIZE_YMM, 8, (opcode & 1) != 0);
            return;
        }
        add_str("hps ");
        add_modrm(REG_SIZE_XMM, 4, 1);
        return;
    case 0x19:
    case 0x1a:
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e:
        get_modrm();
        if (prefix & PREFIX_REPZ) {
            if (opcode == 0x1e) {
                if (modrm == 0xfa) {
                    add_str("endbr64");
                    return;
                }
                if (modrm == 0xfb) {
                    add_str("endbr32");
                    return;
                }
            }
        }
        add_str("hint_nop ");
        add_modrm_mem(data_size);
        return;
    case 0x1f:
        get_modrm();
        add_cmd("nop", data_size);
        add_modrm_mem(data_size);
        return;
    case 0x28:
    case 0x29:
        add_str("mov");
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("apd ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, (opcode & 1) == 0);
            return;
        }
        add_str("aps ");
        add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, (opcode & 1) == 0);
        return;
    case 0x2a:
        if (prefix & PREFIX_REPNZ) {
            get_modrm();
            add_cmd("cvtsi2sd", op_size(modrm));
            add_modrm_mem(data_size);
            add_char(',');
            add_modrm_reg(REG_SIZE_XMM);
            return;
        }
        if (prefix & PREFIX_REPZ) {
            get_modrm();
            add_cmd("cvtsi2ss", op_size(modrm));
            add_modrm_mem(data_size);
            add_char(',');
            add_modrm_reg(REG_SIZE_XMM);
            return;
        }
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("cvtpi2pd ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        add_str("cvtpi2ps ");
        add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
        return;
    case 0x2c:
    case 0x2d:
        switch (opcode) {
        case 0x2c: add_str("cvtt"); break;
        case 0x2d: add_str("cvt"); break;
        }
        if (prefix & PREFIX_REPNZ) {
            add_str("sd2si ");
            add_modrm(data_size, REG_SIZE_XMM, 1);
            return;
        }
        if (prefix & PREFIX_REPZ) {
            add_str("ss2si ");
            add_modrm(data_size, REG_SIZE_XMM, 1);
            return;
        }
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("pd2pi ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        add_str("ps2pi ");
        add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
        return;
    case 0x2e:
    case 0x2f:
        switch (opcode) {
        case 0x2e: add_str("ucomi"); break;
        case 0x2f: add_str("comi"); break;
        }
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("sd ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        add_str("ss ");
        add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
        return;
    case 0x38:
        switch (get_code()) {
        case 0x1c:
            add_str("pabsb ");
            if (prefix & PREFIX_DATA_SIZE) {
                add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
                return;
            }
            add_modrm(REG_SIZE_MMX, REG_SIZE_MMX, 1);
            return;
        case 0x1d:
            add_str("pabsw ");
            if (prefix & PREFIX_DATA_SIZE) {
                add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
                return;
            }
            add_modrm(REG_SIZE_MMX, REG_SIZE_MMX, 1);
            return;
        case 0x1e:
            add_str("pabsd ");
            if (prefix & PREFIX_DATA_SIZE) {
                add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
                return;
            }
            add_modrm(REG_SIZE_MMX, REG_SIZE_MMX, 1);
            return;
        case 0x2b:
            if (prefix & PREFIX_DATA_SIZE) {
                add_str("packusdw ");
                add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
                return;
            }
            break;
        case 0x38:
            if (prefix & PREFIX_DATA_SIZE) {
                add_str("pminsb ");
                add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
                return;
            }
            break;
        case 0x39:
            if (prefix & PREFIX_DATA_SIZE) {
                add_str("pminsd ");
                add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
                return;
            }
            break;
        case 0x3c:
            if (prefix & PREFIX_DATA_SIZE) {
                add_str("pmaxsb ");
                add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
                return;
            }
            break;
        case 0x3d:
            if (prefix & PREFIX_DATA_SIZE) {
                add_str("pmaxsd ");
                add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
                return;
            }
            break;
        case 0x3e:
            if (prefix & PREFIX_DATA_SIZE) {
                add_str("pmaxuw ");
                add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
                return;
            }
            break;
        case 0xf6:
            if (prefix & PREFIX_DATA_SIZE) {
                add_str("adcx ");
                add_modrm(data_size, 1, 0);
                return;
            }
            break;
        }
        break;
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
    case 0x47:
    case 0x48:
    case 0x49:
    case 0x4a:
    case 0x4b:
    case 0x4c:
    case 0x4d:
    case 0x4e:
    case 0x4f:
        add_str("cmov");
        add_ttt(opcode & 0xf);
        add_char(' ');
        add_modrm(data_size, data_size, 1);
        return;
    case 0x51:
    case 0x58:
    case 0x59:
    case 0x5c:
    case 0x5d:
    case 0x5e:
    case 0x5f:
        switch (opcode) {
        case 0x51: add_str("sqrt"); break;
        case 0x58: add_str("add"); break;
        case 0x59: add_str("mul"); break;
        case 0x5c: add_str("sub"); break;
        case 0x5d: add_str("min"); break;
        case 0x5e: add_str("div"); break;
        case 0x5f: add_str("max"); break;
        }
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("pd ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        if (prefix & PREFIX_REPNZ) {
            add_str("sd ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        if (prefix & PREFIX_REPZ) {
            add_str("ss ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        add_str("ps ");
        add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
        return;
    case 0x54:
    case 0x55:
    case 0x56:
    case 0x57:
        switch (opcode) {
        case 0x54: add_str("and"); break;
        case 0x55: add_str("andn"); break;
        case 0x56: add_str("or"); break;
        case 0x57: add_str("xor"); break;
        }
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("pd ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        add_str("ps ");
        add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
        return;
    case 0x5a:
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("cvtpd2ps ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        if (prefix & PREFIX_REPZ) {
            add_str("cvtss2sd ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        if (prefix & PREFIX_REPNZ) {
            add_str("cvtsd2ss ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        add_str("cvtps2pd ");
        add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
        return;
    case 0x5b:
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("cvtps2dq ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        if (prefix & PREFIX_REPZ) {
            add_str("cvttps2dq ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        add_str("cvtdq2ps ");
        add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
        return;
    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
    case 0x68:
    case 0x69:
    case 0x6a:
    case 0x6b:
    case 0x6c:
    case 0x6d:
        {
            static const char * nm[] = {
                "punpcklbw",    "punpcklwd",    "punpckldq",    "packsswb",
                "pcmpgtb",      "pcmpgtw",      "pcmpgtd",      "packuswb",
                "punpckhbw",    "punpckhwd",    "punpckhdq",    "packssdw",
                "punpcklqdq",   "punpckhqdq",
            };
            add_str(nm[opcode - 0x60]);
            add_char(' ');
            if (prefix & PREFIX_DATA_SIZE) {
                add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
                return;
            }
            add_modrm(REG_SIZE_MMX, REG_SIZE_MMX, 1);
        }
        return;
    case 0x6e:
        if (rex & REX_W) add_str("movq ");
        else add_str("movd ");
        if (prefix & PREFIX_DATA_SIZE) {
            add_modrm(REG_SIZE_XMM, data_size, 1);
            return;
        }
        add_modrm(REG_SIZE_MMX, data_size, 1);
        return;
    case 0x6f:
    case 0x7f:
        add_str("mov");
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("dqa ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, (opcode & 0x10) == 0);
            return;
        }
        if (prefix & PREFIX_REPZ) {
            add_str("dqu ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, (opcode & 0x10) == 0);
            return;
        }
        add_str("q ");
        add_modrm(REG_SIZE_MMX, REG_SIZE_MMX, (opcode & 0x10) == 0);
        return;
    case 0x70:
        add_str("pshuf");
        get_modrm();
        if (prefix & PREFIX_REPNZ) {
            add_str("lw ");
            add_imm8(0);
            add_char(',');
            add_modrm_mem(REG_SIZE_XMM);
            add_char(',');
            add_modrm_reg(REG_SIZE_XMM);
            return;
        }
        if (prefix & PREFIX_REPZ) {
            add_str("hw ");
            add_imm8(0);
            add_char(',');
            add_modrm_mem(REG_SIZE_XMM);
            add_char(',');
            add_modrm_reg(REG_SIZE_XMM);
            return;
        }
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("d ");
            add_imm8(0);
            add_char(',');
            add_modrm_mem(REG_SIZE_XMM);
            add_char(',');
            add_modrm_reg(REG_SIZE_XMM);
            return;
        }
        add_str("w ");
        add_imm8(0);
        add_char(',');
        add_modrm_mem(REG_SIZE_MMX);
        add_char(',');
        add_modrm_reg(REG_SIZE_MMX);
        return;
    case 0x71:
        get_modrm();
        switch ((modrm >> 3) & 7) {
        case 2: add_str("psrlw"); break;
        case 4: add_str("psraw"); break;
        case 6: add_str("psllw"); break;
        default: buf_pos = 0; return;
        }
        add_char(' ');
        add_imm8(0);
        add_char(',');
        if (prefix & PREFIX_DATA_SIZE) {
            add_modrm_mem(REG_SIZE_XMM);
        }
        else {
            add_modrm_mem(REG_SIZE_MMX);
        }
        return;
    case 0x72:
        get_modrm();
        switch ((modrm >> 3) & 7) {
        case 2: add_str("psrld"); break;
        case 4: add_str("psrad"); break;
        case 6: add_str("pslld"); break;
        default: buf_pos = 0; return;
        }
        add_char(' ');
        add_imm8(0);
        add_char(',');
        if (prefix & PREFIX_DATA_SIZE) {
            add_modrm_mem(REG_SIZE_XMM);
        }
        else {
            add_modrm_mem(REG_SIZE_MMX);
        }
        return;
    case 0x73:
        get_modrm();
        switch ((modrm >> 3) & 7) {
        case 2: add_str("psrlq"); break;
        case 3: add_str("psrldq"); break;
        case 6: add_str("psllq"); break;
        case 7: add_str("pslldq"); break;
        default: buf_pos = 0; return;
        }
        add_char(' ');
        add_imm8(0);
        add_char(',');
        if (prefix & PREFIX_DATA_SIZE) {
            add_modrm_mem(REG_SIZE_XMM);
        }
        else {
            add_modrm_mem(REG_SIZE_MMX);
        }
        return;
    case 0x74:
        add_str("pcmpeqb ");
        if (prefix & PREFIX_DATA_SIZE) {
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        add_modrm(REG_SIZE_MMX, REG_SIZE_MMX, 1);
        return;
    case 0x76:
        add_str("pcmpeqd ");
        add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
        return;
    case 0x77:
        add_str("emms");
        return;
    case 0x78:
        add_str("vmread ");
        add_modrm(8, 8, 1);
        return;
    case 0x79:
        add_str("vmwrite ");
        add_modrm(8, 8, 1);
        return;
    case 0x7c:
    case 0x7d:
        add_str((opcode & 1) == 0 ? "hadd" : "hsub");
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("pd ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        if (prefix & PREFIX_REPNZ) {
            add_str("ps ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        break;
    case 0x7e:
        if (prefix & PREFIX_REPZ) {
            add_str("movq ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
            return;
        }
        if (rex & REX_W) {
            add_str("movq ");
            add_modrm(REG_SIZE_XMM, 8, 0);
            return;
        }
        add_str("movd ");
        add_modrm(REG_SIZE_XMM, 4, 0);
        return;
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x85:
    case 0x86:
    case 0x87:
    case 0x88:
    case 0x89:
    case 0x8a:
    case 0x8b:
    case 0x8c:
    case 0x8d:
    case 0x8e:
    case 0x8f:
        add_char('j');
        add_ttt(opcode & 0xf);
        add_char(' ');
        add_rel(4);
        return;
    case 0x90:
    case 0x91:
    case 0x92:
    case 0x93:
    case 0x94:
    case 0x95:
    case 0x96:
    case 0x97:
    case 0x98:
    case 0x99:
    case 0x9a:
    case 0x9b:
    case 0x9c:
    case 0x9d:
    case 0x9e:
    case 0x9f:
        get_modrm();
        add_str("set");
        add_ttt(opcode  & 0xf);
        add_char(' ');
        add_modrm_mem(1);
        return;
    case 0xa0:
        add_str("push fs");
        return;
    case 0xa1:
        add_str("pop fs");
        return;
    case 0xa2:
        add_str("cpuid");
        return;
    case 0xa3:
        add_str("bt ");
        add_modrm(data_size, data_size, 0);
        return;
    case 0xa4:
        get_modrm();
        add_str("shld ");
        add_imm8(0);
        add_char(',');
        add_modrm_reg(data_size);
        add_char(',');
        add_modrm_mem(data_size);
        return;
    case 0xa5:
        add_str("shld ");
        add_str("%cl,");
        add_modrm(data_size, data_size, 0);
        return;
    case 0xa8:
        add_str("push gs");
        return;
    case 0xa9:
        add_str("pop gs");
        return;
    case 0xac:
        get_modrm();
        add_str("shrd ");
        add_imm8(0);
        add_char(',');
        add_modrm_reg(data_size);
        add_char(',');
        add_modrm_mem(data_size);
        return;
    case 0xae:
        get_modrm();
        switch ((modrm >> 3) & 7) {
        case 0: add_str("fxsave "); break;
        case 1: add_str("fxrstor "); break;
        case 2: add_str("ldmxcsr "); break;
        case 3: add_str("stmxcsr "); break;
        case 4: add_str("xszve "); break;
        case 5: add_str("lfence"); break;
        case 6: add_str("mfence"); break;
        case 7: add_str("sfence"); break;
        }
        return;
    case 0xaf:
        add_str("imul ");
        add_modrm(data_size, data_size, 1);
        return;
    case 0xb0:
        add_str("cmpxchg ");
        add_modrm(1, 1, 1);
        return;
    case 0xb1:
        add_str("cmpxchg ");
        add_modrm(data_size, data_size, 1);
        return;
    case 0xb3:
        add_str("btr ");
        add_modrm(data_size, data_size, 1);
        return;
    case 0xb6:
        add_cmd("movzb", data_size);
        add_modrm(data_size, 1, 1);
        return;
    case 0xb7:
        add_cmd("movzw", data_size);
        add_modrm(data_size, 2, 1);
        return;
    case 0xba:
        get_modrm();
        switch ((modrm >> 3) & 7) {
        case 0: buf_pos = 0; return;
        case 1: buf_pos = 0; return;
        case 2: buf_pos = 0; return;
        case 3: buf_pos = 0; return;
        case 4: add_cmd("bt", op_size(modrm)); break;
        case 5: add_cmd("bts", op_size(modrm)); break;
        case 6: add_cmd("btr", op_size(modrm)); break;
        case 7: add_cmd("btc", op_size(modrm)); break;
        }
        add_imm8(0);
        add_char(',');
        add_modrm_mem(data_size);
        return;
    case 0xbb:
        add_str("btc ");
        add_modrm(data_size, data_size, 1);
        return;
    case 0xbc:
        add_str("bfr ");
        add_modrm(data_size, data_size, 1);
        return;
    case 0xbd:
        add_str("bsr ");
        add_modrm(data_size, data_size, 1);
        return;
    case 0xbe:
        add_cmd("movsb", data_size);
        add_modrm(data_size, 1, 1);
        return;
    case 0xbf:
        add_cmd("movsw", data_size);
        add_modrm(data_size, 2, 1);
        return;
    case 0xc2:
        get_modrm();
        add_str("cmp");
        switch (get_code()) {
        case 0x00: add_str("lq"); break;
        case 0x01: add_str("lt"); break;
        case 0x02: add_str("le"); break;
        case 0x03: add_str("unord"); break;
        case 0x04: add_str("neq"); break;
        case 0x05: add_str("nlt"); break;
        case 0x06: add_str("nle"); break;
        case 0x07: add_str("ord"); break;
        case 0x08: add_str("eq_uq"); break;
        case 0x09: add_str("nge"); break;
        case 0x0a: add_str("ngt"); break;
        case 0x0b: add_str("false"); break;
        case 0x0c: add_str("neq_oq"); break;
        case 0x0d: add_str("ge"); break;
        case 0x0e: add_str("gt"); break;
        case 0x0f: add_str("true"); break;
        case 0x10: add_str("eq_os"); break;
        case 0x11: add_str("lt_oq"); break;
        case 0x12: add_str("le_oq"); break;
        case 0x13: add_str("unord_s"); break;
        case 0x14: add_str("neq_us"); break;
        case 0x15: add_str("nlt_uq"); break;
        case 0x16: add_str("nle_uq"); break;
        case 0x17: add_str("ord_s"); break;
        case 0x18: add_str("eq_us"); break;
        case 0x19: add_str("nge_uq"); break;
        case 0x1a: add_str("ngt_uq"); break;
        case 0x1b: add_str("false_os"); break;
        case 0x1c: add_str("neq_os"); break;
        case 0x1d: add_str("ge_oq"); break;
        case 0x1e: add_str("gt_oq"); break;
        case 0x1f: add_str("true_us"); break;
        default: buf_pos = 0; return;
        }
        if (prefix & PREFIX_DATA_SIZE) add_str("pd ");
        else if (prefix & PREFIX_REPZ) add_str("ss ");
        else if (prefix & PREFIX_REPNZ) add_str("sd ");
        else add_str("ps ");
        add_modrm_mem(REG_SIZE_XMM);
        add_char(',');
        add_modrm_reg(REG_SIZE_XMM);
        return;
    case 0xc8:
    case 0xc9:
    case 0xca:
    case 0xcb:
    case 0xcc:
    case 0xcd:
    case 0xce:
    case 0xcf:
        add_str("bswap ");
        if (rex & REX_B) add_reg((opcode & 7) + 8, data_size);
        else add_reg(opcode & 7, data_size);
        return;
    case 0xd1:
    case 0xd2:
    case 0xd3:
    case 0xd4:
    case 0xd5:
    case 0xd8:
    case 0xd9:
    case 0xda:
    case 0xdb:
    case 0xdc:
    case 0xdd:
    case 0xde:
    case 0xdf:
    case 0xe0:
    case 0xe1:
    case 0xe2:
    case 0xe3:
    case 0xe4:
    case 0xe5:
    case 0xe8:
    case 0xe9:
    case 0xea:
    case 0xeb:
    case 0xec:
    case 0xed:
    case 0xee:
    case 0xef:
    case 0xf1:
    case 0xf2:
    case 0xf3:
    case 0xf4:
    case 0xf5:
    case 0xf6:
    case 0xf8:
    case 0xf9:
    case 0xfa:
    case 0xfb:
    case 0xfc:
    case 0xfd:
    case 0xfe:
        {
            static const char * nm[] = {
                 NULL,      "psrlw",    "psrld",    "psrlq",    "paddq",    "pmullw",   NULL,       NULL,
                "psubusb",  "psubusw",  "pminub",   "pand",     "paddusb",  "paddusw",  "pmaxb ",   "pandn ",
                "pavgb",    "psraw",    "psrad",    "pavgw",    "pmulhuw",  "pmulhw",   NULL,       NULL,
                "psubsb",   "psubsw",   "pminsw",   "por",      "paddsb",   "paddsw",   "pmaxsw",   "pxor",
                "psllw",    "pslld",    "psllq",    "pmuludq",  "pmaddwd",  "psadbw",   NULL,       "psubb",
                NULL,       "psubw",    "psubd",    "psubq",    "paddb",    "paddw",    "paddd",    NULL,
            };
            const char * s = nm[opcode - 0xd0];
            if (s != NULL) {
                add_str(s);
                add_char(' ');
                if (prefix & PREFIX_DATA_SIZE) {
                    add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 1);
                    return;
                }
                add_modrm(REG_SIZE_MMX, REG_SIZE_MMX, 1);
                return;
            }
        }
        break;
    case 0xe7:
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("movntdq ");
            add_modrm(REG_SIZE_XMM, data_size, 0);
            return;
        }
        add_str("movntq ");
        add_modrm(REG_SIZE_MMX, data_size, 0);
        return;
    case 0xd6:
        if (prefix & PREFIX_DATA_SIZE) {
            add_str("movq ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_XMM, 0);
            return;
        }
        if (prefix & PREFIX_REPZ) {
            add_str("movq2dq ");
            add_modrm(REG_SIZE_MMX, REG_SIZE_XMM, 0);
            return;
        }
        if (prefix & PREFIX_REPNZ) {
            add_str("movdq2q ");
            add_modrm(REG_SIZE_XMM, REG_SIZE_MMX, 0);
            return;
        }
        break;
    }

    buf_pos = 0;
}

static void disassemble_instr(void) {
    uint8_t opcode = 0;
    uint8_t imm = 0;

    opcode = get_code();
    switch (opcode) {
    case 0x00:
    case 0x08:
    case 0x10:
    case 0x18:
    case 0x20:
    case 0x28:
    case 0x30:
    case 0x38:
        add_a_op((opcode >> 3) & 7, 0);
        add_modrm(1, 1, 0);
        return;
    case 0x01:
    case 0x09:
    case 0x11:
    case 0x19:
    case 0x21:
    case 0x29:
    case 0x31:
    case 0x39:
        add_a_op((opcode >> 3) & 7, 0);
        add_modrm(data_size, data_size, 0);
        return;
    case 0x02:
    case 0x0a:
    case 0x12:
    case 0x1a:
    case 0x22:
    case 0x2a:
    case 0x32:
    case 0x3a:
        add_a_op((opcode >> 3) & 7, 0);
        add_modrm(1, 1, 1);
        return;
    case 0x03:
    case 0x0b:
    case 0x13:
    case 0x1b:
    case 0x23:
    case 0x2b:
    case 0x33:
    case 0x3b:
        add_a_op((opcode >> 3) & 7, 0);
        add_modrm(data_size, data_size, 1);
        return;
    case 0x04:
    case 0x0c:
    case 0x14:
    case 0x1c:
    case 0x24:
    case 0x2c:
    case 0x34:
    case 0x3c:
        add_a_op((opcode >> 3) & 7, 0);
        add_imm8(0);
        add_str(",%al");
        return;
    case 0x05:
    case 0x0d:
    case 0x15:
    case 0x1d:
    case 0x25:
    case 0x2d:
    case 0x35:
    case 0x3d:
        add_a_op((opcode >> 3) & 7, 0);
        if (data_size <= 2) add_imm16();
        else add_imm32s(data_size);
        add_char(',');
        add_reg(0, data_size);
        return;
    case 0x06:
        add_str("push es");
        return;
    case 0x07:
        add_str("pop es");
        return;
    case 0x0e:
        add_str("push cs");
        return;
    case 0x0f:
        disassemble_0f();
        return;
    case 0x16:
        add_str("push ss");
        return;
    case 0x17:
        add_str("pop ss");
        return;
    case 0x1e:
        add_str("push ds");
        return;
    case 0x1f:
        add_str("pop ds");
        return;
    case 0x37:
        add_str("aaa");
        return;
    case 0x3f:
        add_str("aas");
        return;
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
    case 0x47:
        add_str("inc ");
        add_reg(opcode & 7, data_size);
        return;
    case 0x48:
    case 0x49:
    case 0x4a:
    case 0x4b:
    case 0x4c:
    case 0x4d:
    case 0x4e:
    case 0x4f:
        add_str("dec ");
        add_reg(opcode & 7, data_size);
        return;
    case 0x50:
    case 0x51:
    case 0x52:
    case 0x53:
    case 0x54:
    case 0x55:
    case 0x56:
    case 0x57:
        add_str("push ");
        if (rex & REX_B) add_reg((opcode & 7) + 8, addr_size);
        else add_reg(opcode & 7, addr_size);
        return;
    case 0x58:
    case 0x59:
    case 0x5a:
    case 0x5b:
    case 0x5c:
    case 0x5d:
    case 0x5e:
    case 0x5f:
        add_str("pop ");
        if (rex & REX_B) add_reg((opcode & 7) + 8, addr_size);
        else add_reg(opcode & 7, addr_size);
        return;
    case 0x63:
        if (x86_mode == MODE_64) {
            add_cmd("movsl", data_size);
            add_modrm(data_size, 4, 1);
        }
        else {
            add_cmd("movsw", data_size);
            add_modrm(data_size, 2, 1);
        }
        return;
    case 0x68:
        if (x86_mode == MODE_64) add_str("pushq ");
        else add_str("push ");
        if (data_size == 2) add_imm16();
        else add_imm32();
        return;
    case 0x69:
        add_str("imul ");
        get_modrm();
        if (data_size <= 2) add_imm16();
        else add_imm32s(data_size);
        add_char(',');
        add_modrm_mem(data_size);
        add_char(',');
        add_modrm_reg(data_size);
        return;
    case 0x6a:
        if (x86_mode == MODE_64) add_str("pushq ");
        else add_str("push ");
        add_imm8(push_size);
        return;
    case 0x6b:
        add_str("imul ");
        get_modrm();
        add_imm8(data_size);
        add_char(',');
        add_modrm_mem(data_size);
        add_char(',');
        add_modrm_reg(data_size);
        return;
    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x75:
    case 0x76:
    case 0x77:
    case 0x78:
    case 0x79:
    case 0x7a:
    case 0x7b:
    case 0x7c:
    case 0x7d:
    case 0x7e:
    case 0x7f:
        add_char('j');
        add_ttt(opcode & 0xf);
        add_char(' ');
        add_rel(1);
        return;
    case 0x80:
        get_modrm();
        data_size = 1;
        add_a_op((modrm >> 3) & 7, op_size(modrm));
        add_imm8(0);
        add_char(',');
        add_modrm_mem(1);
        return;
    case 0x81:
        get_modrm();
        add_a_op((modrm >> 3) & 7, op_size(modrm));
        if (data_size == 2) add_imm16();
        else add_imm32s(data_size);
        add_char(',');
        add_modrm_mem(data_size);
        return;
    case 0x83:
        get_modrm();
        add_a_op((modrm >> 3) & 7, op_size(modrm));
        add_imm8(data_size);
        add_char(',');
        add_modrm_mem(data_size);
        return;
    case 0x84:
        add_str("test ");
        add_modrm(1, 1, 0);
        return;
    case 0x85:
        add_str("test ");
        add_modrm(data_size, data_size, 0);
        return;
    case 0x86:
        add_str("xchg ");
        add_modrm(1, 1, 0);
        return;
    case 0x87:
        add_str("xchg ");
        add_modrm(data_size, data_size, 0);
        return;
    case 0x88:
        add_str("mov ");
        add_modrm(1, 1, 0);
        return;
    case 0x89:
        add_str("mov ");
        add_modrm(data_size, data_size, 0);
        return;
    case 0x8a:
        add_str("mov ");
        add_modrm(1, 1, 1);
        return;
    case 0x8b:
        add_str("mov ");
        add_modrm(data_size, data_size, 1);
        return;
    case 0x8c:
        add_str("mov ");
        add_modrm(REG_SIZE_SEG, data_size, 1);
        return;
    case 0x8d:
        add_str("lea ");
        add_modrm(data_size, 0, 1);
        return;
    case 0x8e:
        add_str("mov ");
        add_modrm(REG_SIZE_SEG, data_size, 0);
        return;
    case 0x8f:
        get_modrm();
        switch ((modrm >> 3) & 7) {
        case 0:
            add_str("pop ");
            add_modrm_mem(data_size);
            return;
        }
        break;
    case 0x90:
        if (prefix & PREFIX_DATA_SIZE) add_str("xchg %ax,%ax");
        else add_str("nop");
        return;
    case 0x98:
        switch (data_size) {
        case 2: add_str("cbtw"); return;
        case 4: add_str("cwtl"); return;
        case 8: add_str("cltq"); return;
        }
        break;
    case 0x99:
        switch (data_size) {
        case 2: add_str("cwtl"); return;
        case 4: add_str("cltd"); return;
        case 8: add_str("cqto"); return;
        }
        break;
    case 0x9a:
        add_str("call ");
        add_imm16();
        add_char(':');
        if (addr_size <= 2) add_imm16();
        else add_imm32();
        return;
    case 0xa0:
        add_str("mov al,");
        add_moffs(0);
        return;
    case 0xa1:
        add_str("mov ");
        add_moffs(1);
        add_char(',');
        add_reg(0, data_size);
        return;
    case 0xa2:
        add_str("mov ");
        add_moffs(0);
        add_char(',');
        add_reg(0, 1);
        return;
    case 0xa3:
        add_str("mov ");
        add_moffs(1);
        add_char(',');
        add_reg(0, data_size);
        return;
    case 0xa4:
    case 0xa5:
        if ((opcode & 1) == 0) data_size = 1;
        if (prefix & PREFIX_REPNZ) add_str("repnz ");
        if (prefix & PREFIX_REPZ) add_str("rep ");
        add_cmd("movs", data_size);
        add_char('%');
        if (prefix & PREFIX_CS) add_char('c');
        else if (prefix & PREFIX_SS) add_char('s');
        else if (prefix & PREFIX_DS) add_char('d');
        else if (prefix & PREFIX_ES) add_char('e');
        else if (prefix & PREFIX_FS) add_char('f');
        else if (prefix & PREFIX_GS) add_char('g');
        else add_char('d');
        add_str("s:(");
        add_reg(6, addr_size);
        add_str("),%es:(");
        add_reg(7, addr_size);
        add_char(')');
        return;
    case 0xa6:
    case 0xa7:
        if ((opcode & 1) == 0) data_size = 1;
        if (prefix & PREFIX_REPNZ) add_str("repnz ");
        if (prefix & PREFIX_REPZ) add_str("repz ");
        add_cmd("cmps", data_size);
        add_str("%es:(");
        add_reg(7, addr_size);
        add_str("),%");
        if (prefix & PREFIX_CS) add_char('c');
        else if (prefix & PREFIX_SS) add_char('s');
        else if (prefix & PREFIX_DS) add_char('d');
        else if (prefix & PREFIX_ES) add_char('e');
        else if (prefix & PREFIX_FS) add_char('f');
        else if (prefix & PREFIX_GS) add_char('g');
        else add_char('d');
        add_str("s:(");
        add_reg(6, addr_size);
        add_char(')');
        return;
    case 0xa8:
        add_str("test ");
        add_imm8(0);
        add_str(",%al");
        return;
    case 0xa9:
        add_str("test ");
        if (data_size <= 2) add_imm16();
        else add_imm32();
        add_char(',');
        add_reg(0, data_size);
        return;
    case 0xaa:
    case 0xab:
        if (prefix & PREFIX_REPNZ) add_str("repnz ");
        if (prefix & PREFIX_REPZ) add_str("rep ");
        add_str("stos ");
        add_reg(0, (opcode & 1) ? data_size : 1);
        add_str(",%es:(");
        add_reg(7, addr_size);
        add_char(')');
        return;
    case 0xac:
    case 0xad:
        if (prefix & PREFIX_REPNZ) add_str("repnz ");
        if (prefix & PREFIX_REPZ) add_str("rep ");
        add_str("lods ");
        add_reg(0, (opcode & 1) ? data_size : 1);
        add_str(",%es:(");
        add_reg(7, addr_size);
        add_char(')');
        return;
    case 0xae:
    case 0xaf:
        if (prefix & PREFIX_REPNZ) add_str("repnz ");
        if (prefix & PREFIX_REPZ) add_str("rep ");
        add_str("scas ");
        add_str("%es:(");
        add_reg(7, addr_size);
        add_str("),");
        add_reg(0, (opcode & 1) ? data_size : 1);
        return;
    case 0xb0:
    case 0xb1:
    case 0xb2:
    case 0xb3:
    case 0xb4:
    case 0xb5:
    case 0xb6:
    case 0xb7:
        add_str("mov ");
        add_imm8(0);
        add_char(',');
        if (rex & REX_B) add_reg((opcode & 7) + 8, 1);
        else add_reg(opcode & 7, 1);
        return;
    case 0xb8:
    case 0xb9:
    case 0xba:
    case 0xbb:
    case 0xbc:
    case 0xbd:
    case 0xbe:
    case 0xbf:
        add_str("mov ");
        if (data_size <= 2) add_imm16();
        else if (data_size <= 4) add_imm32();
        else add_imm64();
        add_char(',');
        if (rex & REX_B) add_reg((opcode & 7) + 8, data_size);
        else add_reg(opcode & 7, data_size);
        return;
    case 0xc0:
    case 0xc1:
        get_modrm();
        if ((opcode & 1) == 0) data_size = 1;
        if (!shift_op((modrm >> 3) & 7, op_size(modrm))) break;
        add_imm8(0);
        add_char(',');
        add_modrm_mem(data_size);
        return;
    case 0xc2:
        add_str("ret ");
        add_imm16();
        return;
    case 0xc3:
        if (prefix & PREFIX_REPNZ) add_str("repnz ");
        if (prefix & PREFIX_REPZ) add_str("repz ");
        add_str("ret");
        if (addr_size == 8) add_char('q');
        return;
    case 0xc6:
        get_modrm();
        switch ((modrm >> 3) & 7) {
        case 0:
            data_size = 1;
            add_cmd("mov", op_size(modrm));
            add_imm8(0);
            add_char(',');
            add_modrm_mem(1);
            return;
        }
        break;
    case 0xc7:
        get_modrm();
        switch ((modrm >> 3) & 7) {
        case 0:
            add_cmd("mov", op_size(modrm));
            if (data_size <= 2) add_imm16();
            else add_imm32s(data_size);
            add_char(',');
            add_modrm_mem(data_size);
            return;
        }
        break;
    case 0xc8:
        add_str("enter ");
        add_imm16();
        add_char(',');
        add_imm8(0);
        return;
    case 0xc9:
        add_cmd("leave", addr_size == 8 ? 8 : 0);
        return;
    case 0xca:
        add_str("ret ");
        add_imm16();
        return;
    case 0xcb:
        add_str("ret");
        return;
    case 0xcc:
        add_str("int3");
        return;
    case 0xcd:
        add_str("int ");
        add_imm8(0);
        return;
    case 0xce:
        add_str("into");
        return;
    case 0xcf:
        if (addr_size == 8) add_str("iretq");
        else if (addr_size == 4) add_str("iretd");
        else add_str("iret");
        return;
    case 0xd0:
    case 0xd1:
        get_modrm();
        if ((opcode & 1) == 0) data_size = 1;
        if (!shift_op((modrm >> 3) & 7, op_size(modrm))) break;
        add_modrm_mem(data_size);
        return;
    case 0xd2:
    case 0xd3:
        get_modrm();
        if ((opcode & 1) == 0) data_size = 1;
        if (!shift_op((modrm >> 3) & 7, op_size(modrm))) break;
        add_str("%cl,");
        add_modrm_mem(data_size);
        return;
    case 0xd9:
        get_modrm();
        switch ((modrm >> 3) & 7) {
        case 0:
            if ((modrm & 0xc0) == 0xc0) {
                add_str("fld st(");
                add_dec_uint32(modrm & 0x3f);
                add_char(')');
                return;
            }
            add_str("fldw ");
            add_modrm_mem(4);
            return;
        case 2:
            if (modrm == 0xd0) {
                add_str("fnop");
                return;
            }
            add_str("fstw ");
            add_modrm_mem(4);
            return;
        }
        break;
    case 0xdb:
        get_modrm();
        switch ((modrm >> 3) & 7) {
        case 0:
            if ((modrm & 0xc0) == 0xc0) {
                add_str("fcmovnb st(");
                add_dec_uint32(modrm & 0x3f);
                add_char(')');
                return;
            }
            add_str("fild ");
            add_modrm_mem(4);
            return;
        case 1:
            if ((modrm & 0xc0) == 0xc0) {
                add_str("fcmovne st(");
                add_dec_uint32(modrm & 0x3f);
                add_char(')');
                return;
            }
            add_str("fisttp ");
            add_modrm_mem(4);
            return;
        case 2:
            if ((modrm & 0xc0) == 0xc0) {
                add_str("fcmovnbe st(");
                add_dec_uint32(modrm & 0x3f);
                add_char(')');
                return;
            }
            add_str("fist ");
            add_modrm_mem(4);
            return;
        case 3:
            if ((modrm & 0xc0) == 0xc0) {
                add_str("fcmovnu st(");
                add_dec_uint32(modrm & 0x3f);
                add_char(')');
                return;
            }
            add_str("fistp ");
            add_modrm_mem(4);
            return;
        case 4:
            if (modrm == 0xe2) {
                if (prefix & PREFIX_FWAIT) add_str("fclex");
                else add_str("fnclex");
                return;
            }
            if (modrm == 0xe3) {
                if (prefix & PREFIX_FWAIT) add_str("finit");
                else add_str("fninit");
                return;
            }
            if (modrm == 0xe4) {
                add_str("fnsetpm");
                return;
            }
            return;
        case 5:
            if ((modrm & 0xc0) == 0xc0) {
                add_str("fucomi st(");
                add_dec_uint32(modrm & 0x3f);
                add_char(')');
                return;
            }
            add_str("fldt ");
            add_modrm_mem(10);
            return;
        case 6:
            add_str("fcomi st(");
            add_dec_uint32(modrm & 0x3f);
            add_char(')');
            return;
        case 7:
            add_str("fstpt ");
            add_modrm_mem(10);
            return;
        }
        break;
    case 0xdc:
        get_modrm();
        switch ((modrm >> 3) & 7) {
        case 0: add_str("fadd "); break;
        case 1: add_str("fmul "); break;
        case 2: add_str("fcom "); break;
        case 3: add_str("fcomp "); break;
        case 4: add_str("fsub "); break;
        case 5: add_str("fsubr "); break;
        case 6: add_str("fdiv "); break;
        case 7: add_str("fdivr "); break;
        }
        if ((modrm & 0xc0) == 0xc0) {
            add_str("st(");
            add_dec_uint32(modrm & 0x3f);
            add_char(')');
            return;
        }
        add_modrm_mem(8);
        return;
    case 0xdd:
        get_modrm();
        switch ((modrm >> 3) & 7) {
        case 0:
            if ((modrm & 0xc0) == 0xc0) {
                add_str("ffree st(");
                add_dec_uint32(modrm & 0x3f);
                add_char(')');
                return;
            }
            add_str("fldl ");
            add_modrm_mem(8);
            return;
        case 1:
            if ((modrm & 0xc0) == 0xc0) {
                add_str("fxch4 st(");
                add_dec_uint32(modrm & 0x3f);
                add_char(')');
                return;
            }
            add_str("fisttp ");
            add_modrm_mem(8);
            return;
        case 2:
            if ((modrm & 0xc0) == 0xc0) {
                add_str("fst st(");
                add_dec_uint32(modrm & 0x3f);
                add_char(')');
                return;
            }
            add_str("fst ");
            add_modrm_mem(8);
            return;
        case 3:
            if ((modrm & 0xc0) == 0xc0) {
                add_str("fstp st(");
                add_dec_uint32(modrm & 0x3f);
                add_char(')');
                return;
            }
            add_str("fstp ");
            add_modrm_mem(8);
            return;
        }
        break;
    case 0xe3:
        switch (data_size) {
        case 2:
            add_str("jcxz ");
            add_rel(1);
            return;
        case 4:
            add_str("jecxz ");
            add_rel(1);
            return;
        case 8:
            add_str("jrcxz ");
            add_rel(1);
            return;
        }
        break;
    case 0xe8:
        add_cmd("call", addr_size == 8 ? 8 : 0);
        add_rel(addr_size <= 2 ? 2 : 4);
        return;
    case 0xd4:
        add_str("aam");
        imm = get_code();
        if (imm != 0x0a) {
            add_char(' ');
            add_0x_hex_uint32(imm);
        }
        return;
    case 0xd5:
        add_str("aad");
        imm = get_code();
        if (imm != 0x0a) {
            add_char(' ');
            add_0x_hex_uint32(imm);
        }
        return;
    case 0xe9:
        if (prefix & PREFIX_REPNZ) add_str("bnd ");
        add_cmd("jmp", addr_size == 8 ? 8 : 0);
        add_rel(addr_size <= 2 ? 2 : 4);
        return;
    case 0xeb:
        add_str("jmp ");
        add_rel(1);
        return;
    case 0xf1:
        add_str("int1");
        return;
    case 0xf4:
        add_str("hlt");
        return;
    case 0xf6:
    case 0xf7:
        get_modrm();
        if (opcode == 0xf6) data_size = 1;
        switch ((modrm >> 3) & 7) {
        case 0:
        case 1:
            add_cmd("test", op_size(modrm));
            if (data_size <= 1) add_imm8(0);
            else if (data_size <= 2) add_imm16();
            else add_imm32s(data_size);
            add_char(',');
            add_modrm_mem(data_size);
            return;
        case 2:
            add_cmd("not", op_size(modrm));
            add_modrm_mem(data_size);
            return;
        case 3:
            add_cmd("neg", op_size(modrm));
            add_modrm_mem(data_size);
            return;
        case 4:
            add_cmd("mul", op_size(modrm));
            add_modrm_mem(data_size);
            return;
        case 5:
            add_cmd("imul", op_size(modrm));
            add_modrm_mem(data_size);
            return;
        case 6:
            add_cmd("div", op_size(modrm));
            add_modrm_mem(data_size);
            return;
        case 7:
            add_cmd("idiv", op_size(modrm));
            add_modrm_mem(data_size);
            return;
        }
        break;
    case 0xfc:
        add_str("cld");
        return;
    case 0xfd:
        add_str("std");
        return;
    case 0xfe:
        get_modrm();
        data_size = 1;
        switch ((modrm >> 3) & 7) {
        case 0:
            add_cmd("inc", op_size(modrm));
            add_modrm_mem(1);
            return;
        case 1:
            add_cmd("dec", op_size(modrm));
            add_modrm_mem(1);
            return;
        }
        break;
    case 0xff:
        get_modrm();
        switch ((modrm >> 3) & 7) {
        case 0:
            add_cmd("inc", op_size(modrm));
            add_modrm_mem(data_size);
            return;
        case 1:
            add_cmd("dec", op_size(modrm));
            add_modrm_mem(data_size);
            return;
        case 2:
            if (prefix & PREFIX_REPNZ) add_str("bnd ");
            if (prefix & PREFIX_DS) add_str("notrack ");
            if (x86_mode == MODE_64) add_str("callq *");
            else add_str("call *");
            add_modrm_mem(addr_size);
            return;
        case 4:
            if (prefix & PREFIX_REPNZ) add_str("bnd ");
            if (prefix & PREFIX_DS) add_str("notrack ");
            if (x86_mode == MODE_64) add_str("jmpq *");
            else add_str("jmp *");
            add_modrm_mem(addr_size);
            return;
        case 6:
            if (x86_mode == MODE_64) add_str("pushq ");
            else add_str("pushl ");
            add_modrm_mem(data_size);
            return;
        }
        break;
    }

    buf_pos = 0;
}

static DisassemblyResult * disassemble_x86(uint8_t * code,
        ContextAddress addr, ContextAddress size, int mode,
        DisassemblerParams * disass_params) {

    static DisassemblyResult dr;

    memset(&dr, 0, sizeof(dr));
    buf_pos = 0;
    code_buf = code;
    code_len = (size_t)size;
    code_pos = 0;

    instr_addr = addr;
    params = disass_params;
    x86_mode = mode;
    prefix = 0;
    vex = 0;
    rex = 0;

    /* Instruction Prefixes */
    while (code_pos < code_len) {
        switch (code_buf[code_pos]) {
        case 0xf0:
            prefix |= PREFIX_LOCK;
            add_str("lock ");
            code_pos++;
            continue;
        case 0xf2:
            prefix |= PREFIX_REPNZ;
            code_pos++;
            continue;
        case 0xf3:
            prefix |= PREFIX_REPZ;
            code_pos++;
            continue;
        case 0x2e:
            prefix |= PREFIX_CS;
            code_pos++;
            continue;
        case 0x36:
            prefix |= PREFIX_SS;
            code_pos++;
            continue;
        case 0x3e:
            prefix |= PREFIX_DS;
            code_pos++;
            continue;
        case 0x26:
            prefix |= PREFIX_ES;
            code_pos++;
            continue;
        case 0x64:
            prefix |= PREFIX_FS;
            code_pos++;
            continue;
        case 0x65:
            prefix |= PREFIX_GS;
            code_pos++;
            continue;
        case 0x66:
            prefix |= PREFIX_DATA_SIZE;
            code_pos++;
            continue;
        case 0x67:
            prefix |= PREFIX_ADDR_SIZE;
            code_pos++;
            continue;
        case 0x9b:
            prefix |= PREFIX_FWAIT;
            code_pos++;
            continue;
        }
        break;
    }

    if (x86_mode == MODE_64) {
        if (code_pos + 1 < code_len && code_buf[code_pos] == 0xc5) { /* Two byte VEX */
            vex = code_buf[code_pos++];
            vex |= (uint32_t)code_buf[code_pos++] << 8;
        }
        else if (code_pos + 2 < code_len && code_buf[code_pos] == 0xc4) { /* Three byte VEX */
            vex = code_buf[code_pos++];
            vex |= (uint32_t)code_buf[code_pos++] << 8;
            vex |= (uint32_t)code_buf[code_pos++] << 16;
        }
        else if (code_pos < code_len && code_buf[code_pos] >= 0x40 && code_buf[code_pos] <= 0x4f) {
            rex = code_buf[code_pos++];
        }
    }

    switch (x86_mode) {
    case MODE_16:
        data_size = prefix & PREFIX_DATA_SIZE ? 4 : 2;
        push_size = prefix & PREFIX_DATA_SIZE ? 4 : 2;
        addr_size = prefix & PREFIX_ADDR_SIZE ? 4 : 2;
        break;
    case MODE_32:
        data_size = prefix & PREFIX_DATA_SIZE ? 2 : 4;
        push_size = prefix & PREFIX_DATA_SIZE ? 2 : 4;
        addr_size = prefix & PREFIX_ADDR_SIZE ? 2 : 4;
        break;
    case MODE_64:
        if (rex & REX_W) {
            data_size = 8;
            push_size = 8;
        }
        else {
            data_size = prefix & PREFIX_DATA_SIZE ? 2 : 4;
            push_size = prefix & PREFIX_DATA_SIZE ? 4 : 8;
        }
        addr_size = prefix & PREFIX_ADDR_SIZE ? 4 : 8;
        break;
    }

    disassemble_instr();

    dr.text = buf;
    if (buf_pos == 0 || code_pos > code_len) {
        snprintf(buf, sizeof(buf), ".byte 0x%02x", code_buf[0]);
        dr.size = 1;
    }
    else {
        buf[buf_pos] = 0;
        dr.size = code_pos;
    }
    return &dr;
}

DisassemblyResult * disassemble_x86_16(uint8_t * code,
    ContextAddress addr, ContextAddress size,
    DisassemblerParams * disass_params) {
    return disassemble_x86(code, addr, size, MODE_16, disass_params);
}

DisassemblyResult * disassemble_x86_32(uint8_t * code,
        ContextAddress addr, ContextAddress size,
        DisassemblerParams * disass_params) {
    return disassemble_x86(code, addr, size, MODE_32, disass_params);
}

DisassemblyResult * disassemble_x86_64(uint8_t * code,
        ContextAddress addr, ContextAddress size,
        DisassemblerParams * disass_params) {
    return disassemble_x86(code, addr, size, MODE_64, disass_params);
}

#endif /* SERVICE_Disassembly */

/*******************************************************************************
 * Copyright (c) 2017 Xilinx, Inc. and others.
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

#ifndef D_cpu_regs_gdb_i386
#define D_cpu_regs_gdb_i386

#include <tcf/config.h>

static const char * cpu_regs_gdb_i386 =
"<architecture>i386</architecture>\n"
"<feature name='org.gnu.gdb.i386.core'>\n"
"  <reg name='eax' bitsize='32' type='int32'/>\n"
"  <reg name='ecx' bitsize='32' type='int32'/>\n"
"  <reg name='edx' bitsize='32' type='int32'/>\n"
"  <reg name='ebx' bitsize='32' type='int32'/>\n"
"  <reg name='esp' bitsize='32' type='data_ptr'/>\n"
"  <reg name='ebp' bitsize='32' type='data_ptr'/>\n"
"  <reg name='esi' bitsize='32' type='int32'/>\n"
"  <reg name='edi' bitsize='32' type='int32'/>\n"
"  <reg name='eip' bitsize='32' type='code_ptr'/>\n"
"  <reg name='eflags' bitsize='32' type='int32'/>\n"
"  <reg name='cs'  bitsize='32' type='int32'/>\n"
"  <reg name='ss'  bitsize='32' type='int32'/>\n"
"  <reg name='ds'  bitsize='32' type='int32'/>\n"
"  <reg name='es'  bitsize='32' type='int32'/>\n"
"  <reg name='fs'  bitsize='32' type='int32'/>\n"
"  <reg name='gs'  bitsize='32' type='int32'/>\n"
"  <reg name='st0' bitsize='80' type='i387_ext'/>\n"
"  <reg name='st1' bitsize='80' type='i387_ext'/>\n"
"  <reg name='st2' bitsize='80' type='i387_ext'/>\n"
"  <reg name='st3' bitsize='80' type='i387_ext'/>\n"
"  <reg name='st4' bitsize='80' type='i387_ext'/>\n"
"  <reg name='st5' bitsize='80' type='i387_ext'/>\n"
"  <reg name='st6' bitsize='80' type='i387_ext'/>\n"
"  <reg name='st7' bitsize='80' type='i387_ext'/>\n"
"  <reg name='fctrl' bitsize='32' type='int' group='float'/>\n"
"  <reg name='fstat' bitsize='32' type='int' group='float'/>\n"
"  <reg name='ftag' bitsize='32' type='int' group='float'/>\n"
"  <reg name='fiseg' bitsize='32' type='int' group='float'/>\n"
"  <reg name='fioff' bitsize='32' type='int' group='float'/>\n"
"  <reg name='foseg' bitsize='32' type='int' group='float'/>\n"
"  <reg name='fooff' bitsize='32' type='int' group='float'/>\n"
"  <reg name='fop' bitsize='32' type='int' group='float'/>\n"
"</feature>\n"
"<feature name='org.gnu.gdb.i386.sse'>\n"
"  <vector id='v4f' type='ieee_single' count='4'/>\n"
"  <vector id='v2d' type='ieee_double' count='2'/>\n"
"  <vector id='v16i8' type='int8' count='16'/>\n"
"  <vector id='v8i16' type='int16' count='8'/>\n"
"  <vector id='v4i32' type='int32' count='4'/>\n"
"  <vector id='v2i64' type='int64' count='2'/>\n"
"  <union id='vec128'>\n"
"    <field name='v4_float' type='v4f'/>\n"
"    <field name='v2_double' type='v2d'/>\n"
"    <field name='v16_int8' type='v16i8'/>\n"
"    <field name='v8_int16' type='v8i16'/>\n"
"    <field name='v4_int32' type='v4i32'/>\n"
"    <field name='v2_int64' type='v2i64'/>\n"
"    <field name='uint128' type='uint128'/>\n"
"  </union>\n"
"  <flags id='i386_mxcsr' size='4'>\n"
"    <field name='IE' start='0' end='0'/>\n"
"    <field name='DE' start='1' end='1'/>\n"
"    <field name='ZE' start='2' end='2'/>\n"
"    <field name='OE' start='3' end='3'/>\n"
"    <field name='UE' start='4' end='4'/>\n"
"    <field name='PE' start='5' end='5'/>\n"
"    <field name='DAZ' start='6' end='6'/>\n"
"    <field name='IM' start='7' end='7'/>\n"
"    <field name='DM' start='8' end='8'/>\n"
"    <field name='ZM' start='9' end='9'/>\n"
"    <field name='OM' start='10' end='10'/>\n"
"    <field name='UM' start='11' end='11'/>\n"
"    <field name='PM' start='12' end='12'/>\n"
"    <field name='FZ' start='15' end='15'/>\n"
"  </flags>\n"
"  <reg name='xmm0' bitsize='128' type='vec128' regnum='32'/>\n"
"  <reg name='xmm1' bitsize='128' type='vec128'/>\n"
"  <reg name='xmm2' bitsize='128' type='vec128'/>\n"
"  <reg name='xmm3' bitsize='128' type='vec128'/>\n"
"  <reg name='xmm4' bitsize='128' type='vec128'/>\n"
"  <reg name='xmm5' bitsize='128' type='vec128'/>\n"
"  <reg name='xmm6' bitsize='128' type='vec128'/>\n"
"  <reg name='xmm7' bitsize='128' type='vec128'/>\n"
"  <reg name='mxcsr' bitsize='32' type='i386_mxcsr' group='vector'/>\n"
"</feature>\n";

#endif /* D_cpu_regs_gdb_i386 */

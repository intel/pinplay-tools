#BEGIN_LEGAL 
#BSD License 
#
#Copyright (c)2022 Intel Corporation. All rights reserved.
#
#Redistribution and use in source and binary forms, with or without modification, 
# are permitted provided that the following conditions are met:
#
#1. Redistributions of source code must retain the above copyright notice, 
#   this list of conditions and the following disclaimer.
#
#2. Redistributions in binary form must reproduce the above copyright notice, 
#   this list of conditions and the following disclaimer in the documentation 
#   and/or other materials provided with the distribution.
#
#3. Neither the name of the copyright holder nor the names of its contributors 
#   may be used to endorse or promote products derived from this software without 
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#END_LEGAL
import sys
import os
import re
import copy

def find_dir(d):
    dir = os.getcwd()
    last = ''
    while dir != last:
        target_dir = os.path.join(dir,d)
        if os.path.exists(target_dir):
            return target_dir
        last = dir
        (dir,tail) = os.path.split(dir)
    return None

sys.path.append(find_dir('mbuild'))
import mbuild


########################################################################
def add_include_dir(env,s):
    mbuild.msgb("ADD INCLUDE HEADER", s)
    env.add_include_dir(env.expand_string(s))

def add_system_include_dir(env,s):
    mbuild.msgb("ADD SYSTEM INCLUDE HEADER", s)
    env.add_system_include_dir(env.expand_string(s))

def find_windows_h(env):
    include_path = os.environ['INCLUDE']
    include_dirs = include_path.split(';')
    for curr_dir in include_dirs:
        if os.path.exists(mbuild.join(curr_dir,'windows.h')):
            return curr_dir
    return None
        
def knobs(env):
    env.parser.add_option('--xedshared', dest='xedshared', action='store_true',
                          default=False,
                          help='Use libxed.{so,dylib} or xed.{lib,dll} ' + 
                               'instead of libxed.a')

def init_common_requirements(env):
    if 'RELIB' not in env:
        env['RELIB'] = []
    if 'standalone_program' not in env:
        env['standalone_program']=False


def find_build_kit_path(env):
    if 'build_kit_path' not in env:
        try:
            env['build_kit_path'] = os.getenv("SDE_BUILD_KIT")
            mbuild.msgb("SDE_BUILD_KIT env var", env['build_kit_path'])
        except:
            mbuild.die("Could not find SDE_BUILD_KIT in the shell environment")

    if not env['build_kit_path']:
        mbuild.die("Could not find build_kit_path is not set properly")

    if os.path.exists(env['build_kit_path']):
        return
    mbuild.die("Could not find build kit at {0:s}".format(
                                               env['build_kit_path']))

def locate_pin_tree(env):

    if 'pin' not in env:
        find_build_kit_path(env)
        env['pin'] = mbuild.join(env['build_kit_path'], 'pinkit')
    if not env['pin']:
        mbuild.die('Pin directory is not setup ' + 
                   'properly in the environment {0}'.format(env['pin']))
    if not os.path.exists(env['pin']):
        mbuild.die('cannot find the PIN directory: {0:s}'.format(env['pin']))
    mbuild.msgb("FOUND PIN KIT", env['pin'])
    env['kit']=True

def init_pin_crt_compile(env):

    # Add PIN CRT macros
    env.add_define('PIN_CRT=1 ')

    # PIN CRT windwos flags
    if env.on_windows():
        env.add_define('_WINDOWS_H_PATH_="%s"' % (find_windows_h(env))) 
        env.add_define('_TIME_T_DEFINED')
        env.add_define('_WCTYPE_T_DEFINED')
        if env['host_cpu'] == 'ia32':
            env.add_define('__i386__') 
        else:
            env.add_define('__LP64__') 

        # Add special flags
        flags = ' /FIinclude/msvc_compat.h /fp:strict ' 
        env['CXXFLAGS'] += flags
        env['CCFLAGS'] += flags
            
    # Add PIN system include headers for CRT                                
    if env['host_cpu'] == 'x86-64':
        bionic_arch = 'x86_64'
        fenv_arch = 'amd64'
    else:
        bionic_arch = 'x86'
        fenv_arch = 'i387'
    pin_sys_header_dirs = [ mbuild.join(env['pin'],'extras','stlport','include'),
                            mbuild.join(env['pin'],'extras'),
                            mbuild.join(env['pin'],'extras','libstdc++','include'),
                            mbuild.join(env['pin'],'extras','crt','include'),
                            mbuild.join(env['pin'],'extras','crt'),
                            mbuild.join(env['pin'],'extras','crt','include','arch-'+bionic_arch),
                            mbuild.join(env['pin'],'extras','crt','include','kernel','uapi'),
                            mbuild.join(env['pin'],'extras','crt','include','kernel','uapi','asm-x86'),
                            mbuild.join(env['pin'],'extras','crt','include',fenv_arch) ]
    for sys_hdir in pin_sys_header_dirs:
        sys_hdir = env.expand_string(sys_hdir)
        add_system_include_dir(env,sys_hdir)

def init_ms_env_compile(env):
    # warning level 3
    flags = ' /W3'
    
    # treat warnings as errors
    flags += ' /WX'
    
    # disable the following warnings:
    #  - C4800: 'type' : forcing value to bool 'true' or 'false'
    #  - C4244: conversion from 'type1' to 'type2', possible loss of data
    #  - C4530: C++ exception handler used, but unwind semantics are 
    #           not enabled. Specify /EHsc
    #  - C4114: same type qualifier used more than once.
    #           Got this error for avx3_funcs_private.h::128 (knlEmu)
    #  - C4267: 'operator' : conversion from 'type1' to 'type2', possible loss 
    #            of data
    #  - C4250: 'class1' : inherits 'class2::member' via dominance
    #  - C4316: object allocated on the heap may not be aligned 64
    #  - C4302: 'conversion' : truncation from 'type 1' to 'type 2'
    #  - C4302: 'result of 32-bit shift implicitly converted to 64 bits'
    #  - C4312: 'type cast': conversion from 'UINT32' to 'WINDOWS::HANDLE' of greater size
    flags += ' /wd4530 /wd4800 /wd4244 /wd4114 /wd4267 /wd4250 /wd4316 /wd4302 /wd4334 /wd4312 '

    # explicitly enable the following warnings:
    #  - C4189: 'identifier' : local variable is initialized but not referenced
    #  - C4101: 'identifier' : unreferenced local variable
    #  - C4701:  potentially uninitialized local variable 'identifier' used
    #  - C4703:  potentially uninitialized local pointer variable 'identifier' used
    #  - C5208:  unnamed class used in typedef name cannot declare members other than
    #            non-static data members, member enumerations, or member classes
    flags += ' /w24189 /w24101 /w24701 /w24703 /wd5208 '
    
    flags += ' /EHs- /EHa- /Gy'
    flags += ' /GS-'        # No security checks
    flags += ' /GR-'        # No RTTI  # (from libs build)
    if env['debug']:
        flags += ' /MDd '     
    else:
        flags += ' /MD '

    # eliminate POSIX deprecation warnings
    flags += ' %(DOPT)s_CRT_NONSTDC_NO_DEPRECATE'
    flags += ' %(DOPT)s_CRT_SECURE_NO_DEPRECATE'

    # Add flag for VS 2017, PinCRT support is missing for it
    flags += ' /Zc:threadSafeInit- ' 

    # encourage the compiler to use sse2, but it will still sometime
    # use x87 on win32
    if env['host_cpu'] == 'ia32':
        flags += ' /arch:SSE2'

    env['CXXFLAGS'] += flags
    env['CCFLAGS'] += flags
    
    if not env['standalone_program']:
        init_pin_crt_compile(env)

def init_ms_env_link_tool(env):
    lflags = ''
    lflags += ' /EXPORT:main'
    lflags += ' /INCREMENTAL:NO'
    lflags += ' /NODEFAULTLIB'
    lflags += ' /NXCompat'
    if env['debug']:
        lflags += ' /DEBUG'
    else:
        lflags += ' /OPT:REF'
    if env['host_cpu'] == 'ia32':
       lflags += ' /entry:Ptrace_DllMainCRTStartup@12'
       lflags += ' /BASE:0x55000000'
    elif env['host_cpu'] == 'x86-64':
       lflags += ' /entry:Ptrace_DllMainCRTStartup'
       lflags += ' /BASE:0xC5000000'

    pin_cpu_bits = env['pin_cpu_bits']

    # PIN CRT Windows link flags
    lflags += ' /NODEFAULTLIB  /IGNORE:4210 /IGNORE:4049 /IGNORE:4217 /FORCE:MULTIPLE '
    lflags += ' /LIBPATH:%s/%s/runtime/pincrt ' % (env['pin'],env['host_cpu'])

    env['LINKFLAGS'] += lflags
    
    if env['shared']:
        crt_libs = "pincrt.lib" 
    else:
        crt_libs = "stlport-static.lib m-static.lib c-static.lib os-apis.lib"

    pin_libs = " pin.lib xed.lib %s kernel32.lib " % (crt_libs)
    env['LIBS'] += pin_libs


def add_sde(env):
    """For build kits"""
    libs = []
    for s in ['libsde']:
        t ='%(pin)s/sde-example/lib/%(sde_arch)s/' + s + "%(LIBEXT)s"
        libs.append(t)
    env['LIBS'] = " ".join(libs) + " "  + env['LIBS']
    add_include_dir(env,'%(pin)s/sde-example/include')
    

def find_file(locs):
    for x in locs:
        if x and os.path.exists(x):
           return x
    return None

def find_linux_archiver(env):
    ar1 = env.expand_string("%(AR)s")
    ar2 = mbuild.join(env.expand_string("%(toolchain)s"),'gar')
    ar3 = mbuild.join(env.expand_string("%(toolchain)s"),'ar')
    ar4 = env.path_search(ar1)
    ar5 = env.path_search('gar')
    ar6 = env.path_search('ar')
    locs = [ar1,ar2,ar3,ar4,ar5,ar6]
    found = find_file(locs)
    if found:
        env['AR'] = found
    else:
        mbuild.die("Cannot find archiver (ar,gar): %s" % ("\n\t".join(locs)))
        
def find_linux_linker(env):
    ld1 = env.expand_string("%(LINK)s")
    ld2 = mbuild.join(env.expand_string("%(toolchain)s"),'gld')
    ld3 = mbuild.join(env.expand_string("%(toolchain)s"),'ld')
    ld4 = env.path_search(ld1)
    ld5 = env.path_search('gld')
    ld6 = env.path_search('ld')
    locs = [ld1,ld2,ld3,ld4,ld5,ld6]
    found = find_file(locs)
    if found:
        env['LINK'] = found
    else:
        mbuild.die("Cannot find linker (ld,gld): %s" % ("\n\t".join(locs)))

def find_path(cmd):   
   mbuild.msgb("Checking location of %s (on PATH)" % (cmd))
   path = os.getenv("PATH")
   for p in path.split(os.pathsep):
       f = os.path.join(p,cmd)
       if os.path.exists(f):
           return p
   # this relies on "which" being available....
   cmd = "which %s" % (cmd)
   (retcode, stdout, stderr) = mbuild.run_command_unbufferred(cmd)
   if retcode == 0:
      return os.path.dirname(stdout[0])
   return 'unknown'

def gcc_supported(gcc):
    # check if gcc runs correctly on the machine
    cmd = gcc + ' --version'
    (retcode, stdout, stderr) = mbuild.run_command(cmd)
    if retcode != 0:
        return False

    # check if gcc linker can run correctly 
    cmd = gcc + ' -Wl,--version'
    (retcode, stdout, stderr) = mbuild.run_command(cmd)
    if retcode != 0:
        return False
    return True

def setup_local_gcc_internal(env, gver, suffix):
   """Setup the gcc toolchain in /usr/local/gcc-*. suffix might be a
   dash and then the version or it might be the empty string."""

   tc  =  '/usr/local/gcc-' + gver + '/bin/'
   gcc = tc + 'gcc' + suffix
   if not os.path.exists(gcc):
       return False

   if not gcc_supported(gcc):
       return False
   
   env['gcc_version']    =  mbuild.compute_gcc_version(gcc)
   env['toolchain']      =  tc
   env['AR']             =  env['toolchain'] + 'ar'
   env['CC_COMPILER']    =  'gcc' + suffix
   env['CXX_COMPILER']   =  'g++' + suffix
   env['ASSEMBLER']      =  'gcc' + suffix
   return True


def setup_local_gcc(env):
   """Try different versions of gcc in priority order"""
   gcc_versions = [ '10.2.1', '10.1.0', '8.3.0' ]
   algorithms  = [ 'no-suffix', 'suffix']
   
   for gver in gcc_versions:
       for alg in algorithms:
           if alg == 'suffix':
               suffix = '-' + gver
           elif alg == 'no-suffix':
               suffix = ''
           if setup_local_gcc_internal(env, gver, suffix):
               return True
   return False

def find_gcc_usr_intel(env):
   for gv in ['10.1.0', '8.2.0']:
       env['gcc_version'] = gv
       p = env.expand('/usr/intel/pkgs/gcc/%(gcc_version)s/bin')
       if os.path.exists(p):
           break
   
def init_gnu_env_compile(env):

    if env['toolchain'] == '':
        # find a reasonable toolchain if none is set
        if not env['gcc_version']:
            # First try local gcc
            if not setup_local_gcc(env):
                # Then try intel packages
                find_gcc_usr_intel(env)
                p = env.expand('/usr/intel/pkgs/gcc/%(gcc_version)s/bin')
                if os.path.exists(p):
                    env['toolchain'] = p + '/'
                else: # search the path
                    gccname = env.expand('%(CC_COMPILER)s')
                    env['toolchain'] =  find_path(gccname) + '/'
                    env['gcc_version'] = mbuild.compute_gcc_version( env['toolchain'] + gccname)

    mbuild.msgb("TOOLCHAIN", env['toolchain'])
    if env['toolchain'] == '':
        mbuild.die("must set toolchain= on the command line")
    if not os.path.exists(env['toolchain']):
        mbuild.die("toolchain not found: %s" % (env['toolchain']))

    flags = ''
    if not env['gcc_version']:
        env['gcc_version'] = mbuild.compute_gcc_version( env['toolchain'] +
                                                         env['CC_COMPILER'])

    flags += ' -fomit-frame-pointer'
    flags += ' -Wall'
    flags += ' -Werror' 
    flags += ' -Wno-unknown-pragmas'
    flags += ' -fno-strict-aliasing'
    flags += ' -Wno-long-long' # from libs
    flags += ' -Wno-unused-function'
    flags += ' -pipe -fmessage-length=0'
    flags += ' -fno-exceptions' # from libs
    flags += ' -fno-stack-protector'
    flags += ' -Wno-missing-braces '
    flags += ' -Wuninitialized -Winit-self -Wmissing-field-initializers '

    flags += ' -Wformat -Wformat-security'  # printf static checks

    # use for C++ only flags
    major_gcc_ver = int(env['gcc_version'].split('.')[0])
    cxxflags = ''
    if env.on_linux() and major_gcc_ver >= 7:
        cxxflags +=  ' -faligned-new '

    if env.on_linux():
        flags += ' -fstack-clash-protection '
        flags += ' -fabi-version=2 '

    # Modify maximal alignment in SDE on MAC to 8
    # This is done as a workaround for a bug in PIN code.   
    if env.on_mac() and env['host_cpu'] == 'ia32':
        flags += ' -fmax-type-align=8'

    # MAC darwin flags
    if env.on_mac():
        flags += ' -D__DARWIN_ONLY_UNIX_CONFORMANCE=1 -D__DARWIN_UNIX03=0 '

    # PIN CRT flags
    if not env['standalone_program']:
        flags += ' -funwind-tables'
    
    if env.on_linux() or env.on_mac():
        find_linux_archiver(env)
        find_linux_linker(env)

    if env.on_linux():
        if not env['standalone_program']:
            if env['host_cpu'] == 'x86-64':
                env['fpic'] = '-fPIC'
                flags += ' %(fpic)s '

    if env['host_cpu'] == 'ia32':
        if env.on_cygwin():
            flags  += ' -mno-cygwin'

    # required for gcc cmpxchg16b intrinsics on 64b systems
    if env.on_linux() and env['host_cpu'] == 'x86-64':
        flags  += ' -mcx16' 

    if env['avx']:
        flags += ' -march=corei7-avx -mavx '
    else:
        # enable sse2/3 on lin32 and lin64
        flags += ' -msse3 '
        if not env.on_mac(): 
            # clang does not support -mfpath=sse
            flags += ' -mfpmath=sse '

    env['CXXFLAGS'] += flags + cxxflags
    env['CCFLAGS'] += flags 
    if not env['standalone_program']: 
        env['CXXFLAGS'] += ' -fno-rtti -Wno-error=shadow -Wno-shadow' 

    if not env['standalone_program']:
        init_pin_crt_compile(env)

def init_gnu_env_link_tool(env):
    lflags = ''
    libs = ''
    if env.on_linux():
        if 'static_pin' in env and env['static_pin']:
            libs += ' -lsapin'
        else:
            libs += ' -lpin'
    if env.on_mac():
        libs += ' -lpin'

    # PIN CRT linker flags
    libs += ' -nostdlib -lc-dynamic -lm-dynamic -lstlport-dynamic '
    libs += ' -lunwind-dynamic '
        
    if env['xedshared']:
        if env['build_os'] == 'win':
            libs += ' %(xed_lib_dir)s/xed%(DLLEXT)s'
        else:
            libs += ' %(xed_lib_dir)s/libxed%(DLLEXT)s'
    else:
        libs += ' -lxed'

    if env['build_os'] == 'mac':
        env['sotool_linker_script'] = '%(pin)s/source/include/pin/pintool.exp' 
        sotool_lflags  = ' -Wl,-exported_symbols_list'
        sotool_lflags += ' -Wl,%(sotool_linker_script)s'

        env['sotool_lflags']= sotool_lflags
        lflags += ' %(sotool_lflags)s '

    if env.on_linux():
        libs   += ' -lpin3dwarf'

        # make the pin tools shared objects on linux
        sotool_lflags = ''
        
        # omitting -shared because linker adds it
        sotool_lflags += ' -Wl,-Bsymbolic'

        # this will result in minimal exported symbols (in stripped binaries)
        # /pin/ required for pin >= r56431.
        env['sotool_linker_script'] = '%(pin)s/source/include/pin/pintool.ver'
        sotool_lflags += ' -Wl,--version-script=%(sotool_linker_script)s'

        env['sotool_lflags']= sotool_lflags
        lflags += ' %(sotool_lflags)s   -Wl,--no-undefined'

        # security related settings
        lflags += ' -z noexecstack'
        lflags += ' -z relro'

    if env['host_os'] in ['lin']:
        if not env['standalone_program']:
            libs   += ' -ldl-dynamic'

    if env['host_cpu'] == 'ia32':
                 
        if env.on_cygwin():
            lflags += ' -mno-cygwin'

        if env['build_os'] == 'win':
            libs   += ' -lpinvm -lntdll'
            lflags += ' -Wl,--export-all-symbols'
            lflags += ' -shared' 
            lflags += ' -Wl,-wrap,atexit,' +  \
                           '-wrap,_onexit,-e,_True_DllMainCRTStartup@12'
            lflags += ' -Wl,--image-base -Wl,0x55000000'
        elif env['host_os'] in ['lin']:
            pass
        elif env['build_os'] == 'mac':
            lflags += ' -w -Wl,-multiply_defined -Wl,suppress'
        else:
            mbuild.die("Unhandled ia32 os: build_os: " + 
                       env['build_os'] +
                       ' / host_os: ' + 
                       env['host_os'] )
                       
    # Enable old linker on Mac and add lpin3dwarf
    if env.on_mac():
        libs   += ' -Wl,-no_new_main'
        libs   += ' -lpin3dwarf'

    env['LINKFLAGS'] += lflags
    env['LIBS'] += libs

    
def init_pintool_compiler_env(env):
    if env['compiler'] == 'ms':
        init_ms_env_compile(env)
        if not env['standalone_program']:
            init_ms_env_link_tool(env)
    elif env['compiler'] in [ 'gnu', 'clang']:
        init_gnu_env_compile(env)
        if not env['standalone_program']:
            init_gnu_env_link_tool(env)
    else:
        mbuild.msgb("Unsupported compiler: %s" % (env['compiler']))

def init_pin_environment_compile(env):
    env['pintool_suffix'] = ''
    env['pintool_suffix'] = env['DLLEXT']

    if env['host_cpu'] == 'x86-64':
        env['pin_arch'] = 'intel64'
        env['sde_arch'] = 'intel64'
        env['arch'] = 'intel64'
    else:
        env['pin_arch'] = 'ia32'
        env['sde_arch'] = 'ia32'
        env['arch'] = 'ia32'
    if 'avx' in env:
        if env['avx']:
            if env['host_cpu'] == 'x86-64':
                env['sde_arch'] = 'intel64-avx'
    else:
        env['avx'] = False
        
    pin_cpu_bits = { 'ia32':'32', 'x86-64':'64', 'ipf':'64' }
    pin_cpu_types = { 'ia32':'ia32', 'x86-64':'ia32e', 'ipf':'ipf' }
    pin_fund_cpu_types = { 'ia32':'ia32', 'x86-64':'intel64', 'ipf':'ia64' }

    pin_os_types = { 'mac':'m', 'lin':'l', 'win':'w' }
    pin_os_targets= { 'mac':'MAC', 'lin':'LINUX', 'win':'WINDOWS' }

    env['pin_cpu_bits'] =  pin_cpu_bits
    pin_cpu = pin_cpu_types[env['host_cpu']].upper()
    pin_fund_cpu = pin_fund_cpu_types[env['host_cpu']].upper()
    pin_os = pin_os_targets[env['host_os']]
    env.add_define('TARGET_' + pin_cpu)
    env.add_define('HOST_'   + pin_cpu)
    env.add_define('TARGET_' + pin_os)

    env.add_define('HOST_'   + pin_os)
    if env.on_mac():
        env.add_define('TARGET_MAC')

    env.add_define('SDE_INIT')
    env.add_define('BIGARRAY_MULTIPLIER=1')
    if env['host_cpu'] != 'ipf':
        env.add_define('USING_XED')

    ############################################################################
    pin_header_dirs = []
    
    pin_header_dirs = [ mbuild.join(env['pin'],'source','include','pin'),
                        mbuild.join(env['pin'],'source','include','pin','gen'),
                        mbuild.join(env['pin'],'source','tools','PinPoints'),
                        mbuild.join(env['pin'],'extras','components','include'),
                        mbuild.join(env['pin'],'extras','xed-%(pin_arch)s',
                                               'include', 'xed') ]

    for hdir in pin_header_dirs:
        hdir = env.expand_string(hdir)
        add_include_dir(env,hdir)
    
    
def init_pintool_environment_link(env):    
    pin_lib_dirs = []

    env['xed_lib_dir'] = mbuild.join(env['pin'],
                                     'extras', 
                                     'xed-%(pin_arch)s', 
                                     'lib') 

    pin_lib_dirs = [ mbuild.join(env['pin'], '%(pin_arch)s','lib'),
                     mbuild.join(env['pin'], '%(pin_arch)s','lib-ext'),
                     mbuild.join(env['pin'], '%(pin_arch)s','runtime','pincrt')]

    pin_lib_dirs.append( '%(xed_lib_dir)s')

    lflags = ''
    for ldir in pin_lib_dirs:
        lflags += ' %(LOPT)s' + ldir
    env['LINKFLAGS'] += lflags


#######################################################
# EXTERNAL INTERFACE
#######################################################

def setup(env):
    mbuild.msgb("Using build_kit.setup")
    init_common_requirements(env)
    locate_pin_tree(env)
    init_pin_environment_compile(env)
    if not env['standalone_program']:
        init_pintool_environment_link(env)
    init_pintool_compiler_env(env)

def early_init(env, build_kit=False):
    """Called before parsing knobs for early init"""
    mbuild.msgb("Using build_kit.early_init")
    knobs(env)
    if not build_kit:
        #The buildkit's mfile is using this script as well.
        #The buildkit does not need the build_config information 
        #and does not have access to it. So we import it only here.
        import build_config 
        env['kit_info'] = build_config.kit_info()

    
def late_init(env):
    """Called after parsing knobs for late init"""
    mbuild.msgb("Using build_kit.late_init")
    setup(env)
    add_sde(env)

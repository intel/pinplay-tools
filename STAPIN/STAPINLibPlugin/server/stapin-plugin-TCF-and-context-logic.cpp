/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "stapin-plugin-TCF-and-context-logic.h"

#include <cstring>

#include <sys/stat.h>

namespace{
    //**************** GLOBALS ************************** */
    std::thread eventThread;
    //***********************FOR TCF INIT AND FORK********************** */

    void start_event_loop_thread()
    {
        eventThread = std::thread(run_event_loop);
    }

    void before_fork_callback()
    {
        exit_event_loop();
        tcfandcontextlogic::wait_for_event_thread();
    }

    extern "C" bool start_tcf_agent(){
        ini_mdep(); 
        ini_events_queue();
        ini_services(nullptr , nullptr);
        start_event_loop_thread();
        return 0 == pthread_atfork(before_fork_callback, start_event_loop_thread, start_event_loop_thread);
    }
    
    

} //anonymus namespace

namespace stapinlogic{

    const char** reg_index_to_name()
    {
        static const char* REG_INDEX_TO_NAME[] {
            "",
            #if defined(TARGET_IA32E)
            "rip",
            "rbp",
            "rsp",
            "rax",
            "rdx",
            "rcx",
            "rbx",
            "rsi",
            "rdi",
            "r8",
            "r9",
            "r10",
            "r11",
            "r12",
            "r13",
            "r14",
            "r15",
            "eflags",
            "seg_es",
            "seg_cs",
            "seg_ss",
            "seg_ds",
            "seg_fs",
            "seg_gs",
            "seg_fs_base",
            "seg_gs_base",
            "mxcsr",
            "mxcsrmask",
            "st0",
            "st1",
            "st2",
            "st3",
            "st4",
            "st5",
            "st6",
            "st7",
            "fop",
            "fip",
            "fcs",
            "foo",
            "fos",
            "xmm0",
            "xmm1",
            "xmm2",
            "xmm3",
            "xmm4",
            "xmm5",
            "xmm6",
            "xmm7",
            "xmm8",
            "xmm9",
            "xmm10",
            "xmm11",
            "xmm12",
            "xmm13",
            "xmm14",
            "xmm15",
            "swd", 
            "cwd"
            #else
            "eip", 
            "ebp", 
            "esp", 
            "eax", 
            "ebx", 
            "ecx", 
            "edx", 
            "ebp", 
            "esi",
            "edi", 
            "eflags",
            "seg_es",
            "seg_cs",
            "seg_ss", 
            "seg_ds", 
            "seg_fs",
            "seg_gs", 
            "mxcsr", 
            "mxcsrmask", 
            "st0", 
            "st1", 
            "st2", 
            "st3",
            "st4", 
            "st5", 
            "st6", 
            "st7", 
            "fop", 
            "fip", 
            "fcs", 
            "foo",
            "fos", 
            "xmm0", 
            "xmm1", 
            "xmm2", 
            "xmm3", 
            "xmm4", 
            "xmm5",
            "xmm6", 
            "xmm7", 
            "xmm8", 
            "xmm9", 
            "xmm10", 
            "xmm11", 
            "xmm12",
            "xmm13", 
            "xmm14", 
            "xmm15"
            #endif
        };
        return REG_INDEX_TO_NAME;
    }

    const size_t reg_index_to_name_size()
    {
        static const size_t REG_INDEX_TO_NAME_SIZE = sizeof(reg_index_to_name()) / sizeof(reg_index_to_name()[0]);
        return REG_INDEX_TO_NAME_SIZE;
    }

    uint64_t get_reg_index(const char* regName)
    {
        auto regNames = reg_index_to_name();
        auto regNamesSize = reg_index_to_name_size();
        for (uint64_t i = 1; i < regNamesSize; ++i)
        {
            if(0 == strcmp(regName, regNames[i]))
            {
                return i;
            }
        }
        //TODO write to log
        return 0;
    }
}

namespace stapinlogic
{
    std::unique_ptr<stapin::RPC_Full_Context> clone_rpc_context(const stapin::RPC_Full_Context* rpcContext) 
    {
        if (nullptr == rpcContext) 
        {
            //TODO write to log
            return nullptr; 
        }
        stapin::RPC_Full_Context* rawPtr = new (std::nothrow) stapin::RPC_Full_Context(*rpcContext);
        if (nullptr == rawPtr) 
        {
            //TODO write to log
            return nullptr;
        }
        return std::unique_ptr<stapin::RPC_Full_Context>(rawPtr);
    }
}


namespace tcfandcontextlogic
{
    //*************************************************** */
    void wait_for_event_thread(){
        if(eventThread.joinable())
        {
            eventThread.join();
        }
    }

    //***********************STAPIN INIT********************** */

    bool TCF_Init(pid_t pid){
        return start_tcf_agent() && stapincontext::create_tcf_context(pid);
    }


    //***********************FORK********************** */

    bool notify_fork_in_child(NATIVE_TID newPID, NATIVE_TID oldPID, const stapin::RPC_Full_Context* rpcContext){
        if(nullptr == rpcContext)
        {
            //TODO write to log
            return false;
        }
        auto removedSuccessfully = stapincontext::remove_proccess_tcf_context(oldPID);
        if(!removedSuccessfully)
        {
            //TODO write to log
        }
        auto createdSuccessfully = stapincontext::create_tcf_context(newPID);
        if(!createdSuccessfully)
        {
            //TODO write to log
        }
        auto addedThread = stapincontext::add_thread_tcf_context(newPID, newPID);
        auto updatedRpcContext = false;
        if(addedThread)
        {
            updatedRpcContext = tcfandcontextlogic::update_rpc_context(newPID, newPID, rpcContext, stapin::FULL_RPC_CONTEXT_SIZE);
        }
        else
        {
            //TODO write to log
        }
        if(!updatedRpcContext)
        {
            //TODO write to log
        }
        return removedSuccessfully && createdSuccessfully && updatedRpcContext;
    }

    //*********************** END OF FORK********************** */

    //***********************SET COTEXT AND IP********************** */

    bool update_rpc_context(NATIVE_TID pid, NATIVE_TID tid, const stapin::RPC_Full_Context* rpcContext, uint16_t size){
        auto rpcContextClone = stapinlogic::clone_rpc_context(rpcContext);
        if(nullptr == rpcContextClone)
        {
            //TODO write to log
            return false;
        }
        return 0 < size && stapincontext::set_reg_rpc_context(pid, tid, rpcContextClone.get(), size);
    }

    bool update_IP(NATIVE_TID pid, NATIVE_TID tid, uint64_t ip){
        return stapincontext::set_ip_rpc_reg_context(pid, tid, ip, stapinlogic::IP_SIZE);
    }
}

namespace stapincontext{

    //***************FOR THE CONTEXT***************** */

    uint8_t* get_register_value(Context* tcfContext, const char* regName) {
            if (nullptr == regName)
            {
                //TODO write to log
                return nullptr;
            }
            auto rpc_context = stapincontext::get_rpc_reg_context(tcfContext);
            #if defined(TARGET_IA32E)
            if (0 == strncmp("rsp", regName, sizeof("rsp") - 1)) return rpc_context->partial_context.RSP;
            else if (0 == strncmp("rbp", regName, sizeof("rbp") - 1)) return rpc_context->partial_context.RBP;
            else if (0 == strncmp("rip", regName, sizeof("rip") - 1)) return rpc_context->partial_context.RIP;
            else if (0 == strncmp("rax", regName, sizeof("rax") - 1)) return rpc_context->partial_context.RAX;
            else if (0 == strncmp("rdx", regName, sizeof("rdx") - 1)) return rpc_context->partial_context.RDX;
            else if (0 == strncmp("rcx", regName, sizeof("rcx") - 1)) return rpc_context->partial_context.RCX;
            else if (0 == strncmp("rbx", regName, sizeof("rbx") - 1)) return rpc_context->partial_context.RBX;
            else if (0 == strncmp("rsi", regName, sizeof("rsi") - 1)) return rpc_context->partial_context.RSI;
            else if (0 == strncmp("rdi", regName, sizeof("rdi") - 1)) return rpc_context->partial_context.RDI;
            else if (0 == strncmp("r8", regName, sizeof("r8") - 1)) return rpc_context->partial_context.R8;
            else if (0 == strncmp("r9", regName, sizeof("r9") - 1)) return rpc_context->partial_context.R9;
            else if (0 == strncmp("r10", regName, sizeof("r10") - 1)) return rpc_context->partial_context.R10;
            else if (0 == strncmp("r11", regName, sizeof("r11") - 1)) return rpc_context->partial_context.R11;
            else if (0 == strncmp("r12", regName, sizeof("r12") - 1)) return rpc_context->partial_context.R12;
            else if (0 == strncmp("r13", regName, sizeof("r13") - 1)) return rpc_context->partial_context.R13;
            else if (0 == strncmp("r14", regName, sizeof("r14") - 1)) return rpc_context->partial_context.R14;
            else if (0 == strncmp("r15", regName, sizeof("r15") - 1)) return rpc_context->partial_context.R15;
            else if (0 == strncmp("fs_base", regName, sizeof("fs_base") - 1)) return rpc_context->partial_context.SEG_FS_BASE;
            else if (0 == strncmp("gs_base", regName, sizeof("gs_base") - 1)) return rpc_context->partial_context.SEG_GS_BASE;
            else if (0 == strncmp("swd", regName, sizeof("swd") - 1)) return rpc_context->partial_context.SEG_FPSW;
            else if (0 == strncmp("cwd", regName, sizeof("cwd") - 1)) return rpc_context->partial_context.SEG_FPCW;
            #else
            if (0 == strncmp("esp", regName, sizeof("esp") - 1)) return rpc_context->partial_context.ESP;
            else if (0 == strncmp("ebp", regName, sizeof("ebp") - 1)) return rpc_context->partial_context.EBP;
            else if (0 == strncmp("eip", regName, sizeof("eip") - 1)) return rpc_context->partial_context.EIP;
            else if (0 == strncmp("eax", regName, sizeof("eax") - 1)) return rpc_context->partial_context.EAX;
            else if (0 == strncmp("ebx", regName, sizeof("ebx") - 1)) return rpc_context->partial_context.EBX;
            else if (0 == strncmp("ecx", regName, sizeof("ecx") - 1)) return rpc_context->partial_context.ECX;
            else if (0 == strncmp("edx", regName, sizeof("edx") - 1)) return rpc_context->partial_context.EDX;
            else if (0 == strncmp("ebp", regName, sizeof("ebp") - 1)) return rpc_context->partial_context.EBP;
            else if (0 == strncmp("esi", regName, sizeof("esi") - 1)) return rpc_context->partial_context.ESI;
            else if (0 == strncmp("edi", regName, sizeof("edi") - 1)) return rpc_context->partial_context.EDI;
            #endif //those are mutual registers for both 32 abd 64 bit 
            else if (0 == strncmp("eflags", regName, sizeof("eflags") - 1)) return rpc_context->partial_context.EFLAGS;
            else if (0 == strncmp("es", regName, sizeof("es") - 1)) return rpc_context->partial_context.SEG_ES;
            else if (0 == strncmp("cs", regName, sizeof("cs") - 1)) return rpc_context->partial_context.SEG_CS;
            else if (0 == strncmp("ss", regName, sizeof("ss") - 1)) return rpc_context->partial_context.SEG_SS;
            else if (0 == strncmp("ds", regName, sizeof("ds") - 1)) return rpc_context->partial_context.SEG_DS;
            else if (0 == strncmp("fs", regName, sizeof("fs") - 1)) return rpc_context->partial_context.SEG_FS;
            else if (0 == strncmp("gs", regName, sizeof("gs") - 1)) return rpc_context->partial_context.SEG_GS;
            else if (0 == strncmp("mxcsr", regName, sizeof("mxcsr") - 1)) return rpc_context->partial_context.MXCSR;
            else if (0 == strncmp("mxcsrmask", regName, sizeof("mxcsrmask") - 1)) return rpc_context->partial_context.MXCSRMASK;
            //fp 
            else if (0 == strncmp("st0", regName, sizeof("st0") - 1)) return rpc_context->ST0;
            else if (0 == strncmp("st1", regName, sizeof("st1") - 1)) return rpc_context->ST1;
            else if (0 == strncmp("st2", regName, sizeof("st2") - 1)) return rpc_context->ST2;
            else if (0 == strncmp("st3", regName, sizeof("st3") - 1)) return rpc_context->ST3;
            else if (0 == strncmp("st4", regName, sizeof("st4") - 1)) return rpc_context->ST4;
            else if (0 == strncmp("st5", regName, sizeof("st5") - 1)) return rpc_context->ST5;
            else if (0 == strncmp("st6", regName, sizeof("st6") - 1)) return rpc_context->ST6;
            else if (0 == strncmp("st7", regName, sizeof("st7") - 1)) return rpc_context->ST7;
            else if (0 == strncmp("fop", regName, sizeof("fop") - 1)) return rpc_context->FPOPCODE;
            else if (0 == strncmp("fip", regName, sizeof("fip") - 1)) return rpc_context->FPIP_OFF;
            else if (0 == strncmp("fcs", regName, sizeof("fcs") - 1)) return rpc_context->FPIP_SEL;
            else if (0 == strncmp("foo", regName, sizeof("foo") - 1)) return rpc_context->FPDP_OFF;
            else if (0 == strncmp("fos", regName, sizeof("fos") - 1)) return rpc_context->FPDP_SEL;
            else if (0 == strncmp("xmm0", regName, sizeof("xmm0") - 1)) return rpc_context->XMM0;
            else if (0 == strncmp("xmm1", regName, sizeof("xmm1") - 1)) return rpc_context->XMM1;
            else if (0 == strncmp("xmm2", regName, sizeof("xmm2") - 1)) return rpc_context->XMM2;
            else if (0 == strncmp("xmm3", regName, sizeof("xmm3") - 1)) return rpc_context->XMM3;
            else if (0 == strncmp("xmm4", regName, sizeof("xmm4") - 1)) return rpc_context->XMM4;
            else if (0 == strncmp("xmm5", regName, sizeof("xmm5") - 1)) return rpc_context->XMM5;
            else if (0 == strncmp("xmm6", regName, sizeof("xmm6") - 1)) return rpc_context->XMM6;
            else if (0 == strncmp("xmm7", regName, sizeof("xmm7") - 1)) return rpc_context->XMM7;
            else if (0 == strncmp("xmm8", regName, sizeof("xmm8") - 1)) return rpc_context->XMM8;
            else if (0 == strncmp("xmm9", regName, sizeof("xmm9") - 1)) return rpc_context->XMM9;
            else if (0 == strncmp("xmm10", regName, sizeof("xmm10") - 1)) return rpc_context->XMM10;
            else if (0 == strncmp("xmm11", regName, sizeof("xmm11") - 1)) return rpc_context->XMM11;
            else if (0 == strncmp("xmm12", regName, sizeof("xmm12") - 1)) return rpc_context->XMM12;
            else if (0 == strncmp("xmm13", regName, sizeof("xmm13") - 1)) return rpc_context->XMM13;
            else if (0 == strncmp("xmm14", regName, sizeof("xmm14") - 1)) return rpc_context->XMM14;
            else if (0 == strncmp("xmm15", regName, sizeof("xmm15") - 1)) return rpc_context->XMM15;
            return nullptr;
        }



    //******************END OF FOR THE CONTEXT *********** */


    size_t get_size_of_full_rpc_context(){
        return stapin::FULL_RPC_CONTEXT_SIZE;
    }
}

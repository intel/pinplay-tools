/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "stapin-context.h"
#include <sys/uio.h>

namespace{
    static std::mutex eventLoopMutex; //event loop mutex
    static std::mutex regContextMutex;
    static std::mutex TCFContexttMutex;

    struct STAPIN_context_extension {
        // uintptr_t reserved;
        pid_t pid;
        // uint8_t padding[sizeof(uintptr_t)-sizeof(pid_t)];
    };
    
    size_t stapinContextExtensionOffset = 0;
    
    #define EXT(ctx) ((STAPIN_context_extension *)((char *)(ctx) + stapinContextExtensionOffset))

    extern "C" {
        #include <assert.h>
        #include <tcf/framework/pid-hash.h>
    };

    const char* isa_def = 
     #if defined(__x86_64__)
        "X86_64";
    #elif defined(__i386__)
        "386";
    #else
        NULL;
    #endif

    size_t rpcRegContextSize = 0;
    constexpr size_t STAPIN_TCF_CONTEXT_EXTENSION_SIZE = sizeof(STAPIN_context_extension);

    struct Update_rpc_context_request : stapinlogic::Async_args {
        stapin::RPC_Full_Context* rpcRegContext; //!< Pointer to the RPC full context
        uint64_t IP;                    //!< Instruction pointer
        size_t size;                    //!< Size of the context
    };

    /**
    * @brief Creates a STAPIN context asynchronously.
    * @param args A pointer to stapinlogic::Async_args structure containing relevant process and thread IDs.
    */
    void create_stapin_context_async(void* args){
        auto asyncArgs = static_cast<stapinlogic::Async_args*>(args);
        auto pid = asyncArgs->pid;
        auto localCtx = create_context(pid2id(pid, 0));
        if(nullptr == localCtx)
        {
            //TODO write to log
            asyncArgs->result = false;
            stapinlogic::notify_conditional_variable(asyncArgs);
            return;
        }
        localCtx->parent = nullptr;
        localCtx->big_endian = 0;
        localCtx->mem = localCtx;
        localCtx->stopped = 0;
        localCtx->mem_access |= MEM_ACCESS_INSTRUCTION;
        localCtx->mem_access |= MEM_ACCESS_DATA;
        localCtx->mem_access |= MEM_ACCESS_USER;
        localCtx->mem_access |= MEM_ACCESS_RD_STOP;
        localCtx->mem_access |= MEM_ACCESS_WR_STOP;
        auto extention = EXT(localCtx);
        extention->pid = pid;
        link_context(localCtx);
        stapinlogic::notify_conditional_variable(asyncArgs);
    }

    /**
    * @brief Adds a thread asynchronously to the given parent process.
    * @param args A pointer to stapinlogic::Async_args structure containing relevant process and thread IDs.
    */
    void add_thread_async(void* args){
        auto asyncArgs = static_cast<stapinlogic::Async_args*>(args);
        auto parent_pid = asyncArgs->pid;
        auto tid = asyncArgs->tid;
        auto parent = stapincontext::get_TCF_context(parent_pid==tid? 0: parent_pid, parent_pid);
        if(nullptr == parent)
        {
            //TODO write to log
            asyncArgs->result = false;
            stapinlogic::notify_conditional_variable(asyncArgs);
            return;
        }
        auto tcfContext = create_context(pid2id(tid, EXT(parent)->pid));
        if(nullptr == tcfContext)
        {
            //TODO write to log
            asyncArgs->result = false;
            stapinlogic::notify_conditional_variable(asyncArgs);
            return;
        }
        EXT(tcfContext)->pid = tid;
        tcfContext->mem = parent;
        tcfContext->name = loc_printf("%d", tid);
        tcfContext->big_endian = parent->big_endian;
        tcfContext->reg_access |= REG_ACCESS_RD_STOP;
        tcfContext->reg_access |= REG_ACCESS_WR_STOP;
        (tcfContext->parent = parent)->ref_count++;
        list_add_last(&tcfContext->cldl, &parent->children);
        link_context(tcfContext);
        stapinlogic::notify_conditional_variable(asyncArgs);
    }

    /**
    * @brief Removes a thread asynchronously.
    * @param args A pointer to stapinlogic::Async_args structure containing relevant process and thread IDs.
    */
    void remove_thread_async(void* args){
        auto asyncArgs = static_cast<stapinlogic::Async_args*>(args);
        auto pid = asyncArgs->pid;
        auto tid = asyncArgs->tid;
        auto tcfContext = stapincontext::get_TCF_context(pid, tid);
        if(nullptr == tcfContext)
        {
            //TODO write to log
            asyncArgs->result = false;
            stapinlogic::notify_conditional_variable(asyncArgs);
            return;
        }
        tcfContext->exited = 1; 
        context_unlock(tcfContext); //this also reduces the parent ref count
        stapinlogic::notify_conditional_variable(asyncArgs);
    }

    /**
    * @brief Removes a context asynchronously.
    * @param args A pointer to stapinlogic::Async_args structure containing relevant process and thread IDs.
    */
    void remove_context_async(void* args){
        auto asyncArgs = static_cast<stapinlogic::Async_args*>(args);
        auto oldPID = asyncArgs->pid;
        auto tcfContext = stapincontext::get_TCF_context(oldPID, oldPID)->parent; //to get the main one
        if(nullptr == tcfContext)
        {   
            //TODO write to log
            asyncArgs->result = false;
            stapinlogic::notify_conditional_variable(asyncArgs);
            return;
        }
        tcfContext->exiting = 1;
        if (!list_is_empty(&tcfContext->children)) {
            LINK * l = tcfContext->children.next;
            while (l != &tcfContext->children) {
                auto child = cldl2ctxp(l);
                if (!child->exited) {
                    //unlock the child
                    child->exited = 1; 
                    context_unlock(child);
                }
                l = l->next;
            }
        }
        tcfContext->exited = 1;
        context_unlock(tcfContext);
        asyncArgs->result = true;
        stapinlogic::notify_conditional_variable(asyncArgs);
    }

    /**
    * @brief Sets the register context asynchronously.
    * @param arg A pointer to Update_rpc_context_request structure containing relevant process and thread IDs and register context.
    */
    void set_reg_context_async(void* arg) {
        auto updateRegRequest = static_cast<Update_rpc_context_request*>(arg);
        auto pid = updateRegRequest->pid;
        auto tid = updateRegRequest->tid;
        auto rpcRegContext = updateRegRequest->rpcRegContext;
        auto size = updateRegRequest->size;
        auto tcfContext = stapincontext::get_TCF_context(pid, tid);
        if (nullptr == tcfContext || size > rpcRegContextSize) {
            //TODO write to log
            updateRegRequest->result = false;
            stapinlogic::notify_conditional_variable(updateRegRequest);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(regContextMutex); // Guard the memcpy operation
            memcpy(EXT(tcfContext) + STAPIN_TCF_CONTEXT_EXTENSION_SIZE, rpcRegContext, size);
        }
        updateRegRequest->result = true;
        stapinlogic::notify_conditional_variable(updateRegRequest);
        
    }

    /**
    * @brief Sets the instruction pointer register context asynchronously.
    * @param arg A pointer to Update_rpc_context_request structure containing relevant process and thread IDs and instruction pointer.
    */
    void set_ip_reg_context_async(void* arg){
        auto updateRegRequest = static_cast<Update_rpc_context_request*>(arg);
        auto pid = updateRegRequest->pid;
        auto tid = updateRegRequest->tid;
        auto IP = updateRegRequest->IP;
        auto size = updateRegRequest->size;
        auto tcfContext = stapincontext::get_TCF_context(pid, tid);
        if (nullptr == tcfContext || size > rpcRegContextSize) {
            //TODO write to log
            updateRegRequest->result = false;
            stapinlogic::notify_conditional_variable(updateRegRequest);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(regContextMutex); // Guard the memcpy operation
            memcpy(EXT(tcfContext)+STAPIN_TCF_CONTEXT_EXTENSION_SIZE, (void*)IP, size);
        }
        updateRegRequest->result = true;
        stapinlogic::notify_conditional_variable(updateRegRequest);
    }

    

} // anonymus namespace

namespace stapinlogic
{
    void push_func_to_event_loop(EventCallBack * handler, void * arg)
    {
        std::lock_guard<std::mutex> lock(eventLoopMutex);
        post_event(handler, arg);
    }
}

namespace stapincontext{
    bool create_tcf_context(pid_t pid) {
        stapinlogic::Async_args asyncArgs{0};
        std::unique_lock<std::mutex> lock(asyncArgs.mtx); 
        asyncArgs.pid = pid;
        asyncArgs.tid = 0;
        asyncArgs.result = true;
        asyncArgs.ready = false;
        stapinlogic::push_func_to_event_loop(create_stapin_context_async, &asyncArgs);
        asyncArgs.cv.wait(lock, [&asyncArgs] { return asyncArgs.ready; });
        return asyncArgs.result;
    }

    bool add_thread_tcf_context(pid_t pid, pid_t tid){
        stapinlogic::Async_args asyncArgs{0};
        std::unique_lock<std::mutex> lock(asyncArgs.mtx); 
        asyncArgs.pid = pid;
        asyncArgs.tid = tid;
        asyncArgs.result = true;
        asyncArgs.ready = false;
        stapinlogic::push_func_to_event_loop(add_thread_async,&asyncArgs );
        asyncArgs.cv.wait(lock, [&asyncArgs] { return asyncArgs.ready; });
        return asyncArgs.result;
    }

    bool remove_thread_tcf_context(pid_t pid, pid_t tid){
        stapinlogic::Async_args asyncArgs{0};
        std::unique_lock<std::mutex> lock(asyncArgs.mtx); 
        asyncArgs.pid = pid;
        asyncArgs.tid = tid;
        asyncArgs.result = true;
        asyncArgs.ready = false;
        stapinlogic::push_func_to_event_loop(remove_thread_async, &asyncArgs );
        asyncArgs.cv.wait(lock, [&asyncArgs] { return asyncArgs.ready; });
        return asyncArgs.result;
    }

    bool remove_proccess_tcf_context(pid_t oldPID){
        stapinlogic::Async_args asyncArgs{0};
        std::unique_lock<std::mutex> lock(asyncArgs.mtx); 
        asyncArgs.pid = oldPID;
        asyncArgs.result = false;
        asyncArgs.ready = false;
        stapinlogic::push_func_to_event_loop(remove_context_async,&asyncArgs );
        asyncArgs.cv.wait(lock, [&asyncArgs] { return asyncArgs.ready; });
        return asyncArgs.result;
    }

    Context * get_TCF_context(pid_t pid, pid_t tid){
        std::lock_guard<std::mutex> lock(TCFContexttMutex);
        return id2ctx(pid2id(tid, pid));
    }

    void make_regions_from_sections(const stapinlogic::t_memory_region_list& sections, MemoryRegion* result){
        memset(result, 0, sizeof(MemoryRegion) * sections.size());
        size_t section_index = 0;
        for (auto it = sections.begin(); it != sections.end(); ++it, ++section_index) {
            MemoryRegion * cur_region = result + section_index;
            cur_region->addr = it->addr ;
            cur_region->valid = it->valid;
            cur_region->size = it->size;
            cur_region->flags = it->flags;
            cur_region->file_offs = it->fileOffs;
            cur_region->file_size = it->fileSize;
            cur_region->dev = it->dev;
            cur_region->ino = it->ino;
            cur_region->file_name = loc_strdup(it->fileName.get()->c_str());
            cur_region->sect_name = loc_strdup(it->sectionName.c_str());
        }
    }

    stapin::RPC_Full_Context* get_rpc_reg_context(Context* tcfContext){
        return (stapin::RPC_Full_Context*)(EXT(tcfContext)+STAPIN_TCF_CONTEXT_EXTENSION_SIZE);
    }

    bool set_reg_rpc_context(pid_t pid, pid_t tid, stapin::RPC_Full_Context* rpcRegContext, size_t size){
        Update_rpc_context_request updateRegRequest{0};
        std::unique_lock<std::mutex> lock(updateRegRequest.mtx); 
        updateRegRequest.pid = pid;
        updateRegRequest.tid = tid;
        updateRegRequest.rpcRegContext = rpcRegContext;
        updateRegRequest.size = size;
        updateRegRequest.result = false;
        updateRegRequest.ready = false;
        stapinlogic::push_func_to_event_loop(set_reg_context_async, &updateRegRequest );
        updateRegRequest.cv.wait(lock, [&updateRegRequest] { return updateRegRequest.ready; });
        return updateRegRequest.result;
    }

    bool set_ip_rpc_reg_context(pid_t pid, pid_t tid, uint64_t ip, size_t size){
        Update_rpc_context_request updateRegRequest{0};
        std::unique_lock<std::mutex> lock(updateRegRequest.mtx); 
        updateRegRequest.pid = pid;
        updateRegRequest.tid = tid;
        updateRegRequest.IP = ip;
        updateRegRequest.size = size;
        updateRegRequest.result = true;
        updateRegRequest.ready = false;
        stapinlogic::push_func_to_event_loop(set_ip_reg_context_async,&updateRegRequest);
        updateRegRequest.cv.wait(lock, [&updateRegRequest] { return updateRegRequest.ready; });
        return updateRegRequest.result;
    }

    void notify_stop_context(Context* ctx)
    {
        std::lock_guard<std::mutex> lock(TCFContexttMutex);
        ctx->stopped = 1;
    }

    void notify_resume_context(Context* ctx)
    {
        std::lock_guard<std::mutex> lock(TCFContexttMutex);
        ctx->stopped = 0;
    }
};

/**
 * THIS IS TCF CONTEXT INTERFACE FUNCTIONS
 */
int context_get_memory_map(Context* ctx, MemoryMap* mm) {
    const auto& sections = imagelogic::get_sections_from_plugin();
    mm->region_cnt = sections.size();
    if(sections.size() > mm->region_max)
    {
        mm->regions = (MemoryRegion *)loc_realloc(mm->regions, sections.size()*sizeof(MemoryRegion) * sections.size());
        if(nullptr == mm->regions)
        {
            return -1;
        }
        mm->region_max = sections.size();
    }
    stapincontext::make_regions_from_sections(sections, mm->regions);
    return 0;
}

int context_read_reg(Context * ctx, RegisterDefinition * def,
            unsigned offs, unsigned size, void * buf) {
    auto name = def->name;
    auto ext = EXT(ctx);
    auto regVal = ctx->parent? stapincontext::get_register_value(ctx, name): stapincontext::get_register_value(ctx, name);
    if(regVal == nullptr) 
    {
        //TODO write to log
        return -1;
    }
    memcpy(buf, regVal, size);
    return 0;
}

int context_get_isa(Context*, ContextAddress, ContextISA* isa) {
    const char * s = NULL;
    memset(isa, 0, sizeof(ContextISA));
    isa->def = isa_def;
    s = isa->isa ? isa->isa : isa->def;
    if (s) 
    {
        if (0 == strncmp("X86_64", s, sizeof("X86_64")-1))
        {
            isa->max_instruction_size = stapinlogic::MAX_INST_SIZE;
        }
        else if (0 == strncmp("386", s, sizeof("386")-1)) 
        {
            isa->max_instruction_size = stapinlogic::MAX_INST_SIZE;
        }
    }
    return 0;
}

unsigned context_word_size(Context* ctx) {
    return sizeof(void*);
}

int context_get_canonical_addr(Context * ctx, ContextAddress addr,
            Context ** canonical_ctx, ContextAddress * canonical_addr,
            ContextAddress * block_addr, ContextAddress * block_size){
                return 0;
            }

int context_has_state(Context * ctx) {
    return true;
}

Context* context_get_group(Context * ctx, int group) {
    return ctx;
}        

int context_write_mem(Context * ctx, ContextAddress address, void * buf, size_t size) {
    return 0;
}

int context_read_mem(Context * ctx, ContextAddress address, void * buf, size_t size) {
    struct iovec local_iov;
    struct iovec remote_iov;

    local_iov.iov_base = buf;
    local_iov.iov_len = size;
    remote_iov.iov_base = (void *)address;
    remote_iov.iov_len = size;

    ssize_t nread = process_vm_readv(ctx->parent? EXT(ctx->parent)->pid : EXT(ctx)->pid
                                , &local_iov, 1, &remote_iov, 1, 0);
    if (-1 == nread) {
        //TODO write to log
        return -1; 
    }

    return 0; // Success
}

void init_contexts_sys_dep(void) {
    rpcRegContextSize = stapincontext::get_size_of_full_rpc_context();
    stapinContextExtensionOffset = context_extension(STAPIN_TCF_CONTEXT_EXTENSION_SIZE + rpcRegContextSize);
    ini_context_pid_hash();
    return;
}
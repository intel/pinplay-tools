/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef STAPIN_CONTEXT_H
#define STAPIN_CONTEXT_H

#include <condition_variable>
#include <mutex>
#include <list>
#include <string>

extern "C" {
#include <tcf/framework/context.h>
#include <tcf/framework/events.h>
#include <tcf/framework/exceptions.h>
#include <tcf/framework/myalloc.h>
#include <tcf/services/memorymap.h>
#include <tcf/services/linenumbers.h>
#include <tcf/services/stacktrace.h>
#include <tcf/services/symbols.h>
#include <tcf/services/runctrl.h>
#include <tcf/services/expressions.h>
#include <tcf/main/services.h>
#include <tcf/main/framework.h>
};

/*Forward decleration*/
namespace stapin{
    struct RPC_Full_Context;
};

namespace stapinlogic{
    constexpr int MAX_INST_SIZE = 15;

    struct Async_args {
        pid_t pid;                      //!< Process ID
        pid_t tid;                      //!< Thread ID
        std::condition_variable cv;     //!< Condition variable for synchronization
        std::mutex mtx;                 //!< Mutex for synchronization
        bool result;                    //!< Result of the retrieval operation
        bool ready;                     //!< Flag indicating if the retrieval is ready (cv used)
    };
    
    struct STAPIN_memory_region {
        uint64_t addr;                  //!< Region address in context memory
        uint64_t size;                  //!< Region size
        uint64_t fileOffs;              //!< File offset of the region
        uint64_t fileSize;              //!< File size of the region
        int bss;                        //!< 1 if the region is BSS segment
        unsigned long dev;              //!< Region file device ID
        unsigned long ino;              //!< Region file inode
        std::shared_ptr<std::string> fileName;           //!< A shared pointer to the Region file name.
        std::string sectionName;        //!< Region file section name
        unsigned flags;                 //!< Region flags
        unsigned valid;                 //!< Region valid flags
        std::string query;              //!< If not NULL, the region is only part of the memory map for contexts matches the query
    };

    typedef std::list<stapinlogic::STAPIN_memory_region> t_memory_region_list;

    struct Memory_region_comparator  {
        bool operator()(const STAPIN_memory_region& lhs, const STAPIN_memory_region& rhs) const {
            return lhs.addr < rhs.addr;  //!< Order by address
        }
    };

    /**
    * @brief Pushes a function to the TCF event loop. 
    * @details locks a mutex for the post event and releasing it right after. 
    * @param handler A function to push. 
    * @param arg the argument for the pushed function.
    */
    void push_func_to_event_loop(EventCallBack * handler, void * arg);

    /**
    * @brief Notifying the cv when finishing the function.
    * @details lock gurad the mutex, sets the ready flag to true and notifies the cv. 
    * @tparam SYNC_STRUCT the struct that holds the cv, mutex and ready flag to use.
    * @param syncStruct The structure that holds the mutex, the cv and the ready flag. 
    */
    template<typename SYNC_STRUCT>
    void notify_conditional_variable(SYNC_STRUCT syncStruct)
    {
        std::lock_guard<std::mutex> lock(syncStruct->mtx);
        syncStruct->ready = true; // Set the ready flag
        syncStruct->cv.notify_one();
    }
};

namespace imagelogic
{
    /**
    * @brief Retrives memory sections that are saved in the plugin.
    * @details Makes from the memory sections memory regions which is a staructure compatible with TCF.
    * @return an ordered list of memory regions.
    */
    stapinlogic::t_memory_region_list& get_sections_from_plugin(); 
};

namespace stapincontext{
    /**
    * @brief Retrives register value by name and by the relevant TCF context.
    * @param tcfContext A pointer to tcf Context.
    * @param regName A name of the a register. 
    * @return the value of the register if found, null else.
    */
    uint8_t*  get_register_value(Context* tcfContext, const char* regName);

    /**
    * @brief Creates a tcf context for a given proccess ID.
    * @param pid proccess ID. 
    */
    bool create_tcf_context(pid_t pid);

    /**
    * @brief Adding a thread context.
    * @details Creating context for the thread as a child of the context of the proccess ID.
    * @param pid proccess ID. 
    * @param tid Thread ID.
    */
    bool add_thread_tcf_context(pid_t pid, pid_t tid);

    /**
    * @brief Removes  and disposes a thread context.
    * @param pid proccess ID. 
    * @param tid Thread ID.
    */
    bool remove_thread_tcf_context(pid_t pid, pid_t tid);

    /**
    * @brief Removes  and disposes a context.
    * @param oldPID proccess ID. 
    */
    bool remove_proccess_tcf_context(pid_t oldPID);

    /**
    * @brief Retriving a context by its proccess ID and thread ID. 
    * @param pid proccess ID. 
    * @param tid Thread ID.
    * @return Context* which is the result context.
    */
    Context * get_TCF_context(pid_t pid, pid_t tid);

    /**
    * @brief Retriving the size of a full context. 
    * @return size_t which is the result size.
    */
    size_t get_size_of_full_rpc_context();

    /**
    * @brief Retriving the context of regs from a TCF context. 
    * @param tcfContext Context* The context to get the Reg context from.
    * @return stapin::RPC_Full_Context* which is the result context.
    */
    stapin::RPC_Full_Context* get_rpc_reg_context(Context* tcfContext);

    /**
    * @brief Setting the reg-context of a TCF-context that is related to pid and tid. 
    * @param pid proccess ID. 
    * @param tid Thread ID.
    * @param rpcRegContext reg context to set.
    * @param size Size of reg context.
    */
    bool set_reg_rpc_context(pid_t pid, pid_t tid, stapin::RPC_Full_Context*  rpcRegContext, size_t size);

    /**
    * @brief Setting the IP reg of the reg-context of a TCF-context that is related to pid and tid. 
    * @param pid proccess ID. 
    * @param tid Thread ID.
    * @param ip value of IP to set.
    * @param size Size of the IP.
    */
    bool set_ip_rpc_reg_context(pid_t pid, pid_t tid, uint64_t ip, size_t size);

    /**
    * @brief Notifying the context that is has "stopped". 
    * @param ctx context to notify. 
    */
    void notify_stop_context(Context* ctx);

    /**
    * @brief Notifying the context that is has "resumed". 
    * @param ctx context to notify. 
    */
    void notify_resume_context(Context* ctx); 
};



#endif 
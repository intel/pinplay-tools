/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifndef TCF_AND_CONTEXT_LOGIC_H
#define TCF_AND_CONTEXT_LOGIC_H

#include "server-utils.h"

#include <thread>
#include <unordered_map>

#include <cstdint>

namespace stapinlogic{
    //**************** CONSTS ************************** */
    constexpr auto IP_SIZE  = sizeof(uintptr_t);
    constexpr uint16_t MAX_UINQUE_IDS_BUFFER_SIZE = 13000;

    //**************** HELPER FUNCTIONS ************************** */
    /**
     * @brief Cloning Context.
     * @param rpcContext Context to clone.
     * @return Unique ptr of the cloned Context.
     */
    std::unique_ptr<stapin::RPC_Full_Context> clone_rpc_context(const stapin::RPC_Full_Context* rpcContext);

    //**************** REGISTER MAPPINGS ************************** */
    
};

namespace tcfandcontextlogic
{
    /*
    If the event thread is joinable, join (wait).
    */
    void wait_for_event_thread();

    /**
    * @brief 
    Initiate the TCF server including initiating the framework and needed services and starting the event loop.
    Parameters:
    - @param pid: the working proccess ID.
    * @return
    - true if the sTCF server intiated successfully or false otherwise.
    */
    bool TCF_Init(pid_t pid);

    /**
     * @brief
     * Notifying about fork call. 
     * @details
     * This function removes all things assiciated with the oldPID and creates tcf context for the new PID and then creates a thread for it (same PID) and updates its registers.
     * @param newPID A NATIVE_TID representing the new process ID to be associated with the stapin rpc context.
     * @param oldPID A NATIVE_TID representing the old process ID whose associated context should be removed.
     * @param rpcContext A pointer to stapin::RPC_Full_Context representing the stapin rpc context to be associated with the new process ID.
     * 
     * @return
     * - true if the rpc context is successfully created and removed the old.
     * - false otherwise.
     */
    bool notify_fork_in_child(NATIVE_TID newPID, NATIVE_TID oldPID, const stapin::RPC_Full_Context* rpcContext);

    /**
     * @brief
     * Sets or updates the rpc context for a given process and thread ID in the relevant TCF context.
     * @details
     * This function either updates an existing rpc context or creates a new one for the specified process and thread ID.
     * The rpc context is represented by a pointer to stapin::RPC_Full_Context (rpcContext).
     * @param pid A NATIVE_TID representing the process ID.
     * @param tid A NATIVE_TID representing the thread ID.
     * @param rpcContext A pointer to stapin::RPC_Full_Context representing the rpc context to be associated with the process and thread ID.
     * @param size The size of the rpc context in bytes.
     * @return
     * - true if the rpc context is successfully set or updated.
     * - false if any input parameter is invalid.
     */
    bool update_rpc_context(NATIVE_TID pid, NATIVE_TID tid, const stapin::RPC_Full_Context* rpcContext, uint16_t size);

    /**
     * @brief
     * Updates the instruction pointer (IP) for a given process and thread ID in the relevant TCF context.
     * @details
     * This function updates the IP in the rpc context for the specified process and thread ID pair if an existing tcf context is found.
     * @param pid A NATIVE_TID representing the process ID.
     * @param tid A NATIVE_TID representing the thread ID.
     * @param ip A uint64_t representing the instruction pointer to be set in the rpc context.
     * @return
     * - true if the IP is successfully set in the rpc context.
     * - false if any input parameter is zero, or if no tcf context exists for the given process and thread ID pair.
     */
    bool update_IP(NATIVE_TID pid, NATIVE_TID tid, uint64_t ip);
};


#endif
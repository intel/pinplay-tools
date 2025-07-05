/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * @file
 * Message schemas for STAPIN
 *
 * @defgroup STAPIN_SCHEMA STAPIN Message Schema Definitions
 */

#ifndef _STAPIN_SCHEMAS_H_
#define _STAPIN_SCHEMAS_H_

#include <cstdlib> 
#include <cstdint>

#include <rscschema.h>
#include <types.h>


//size of regs and fpstate to let the plugin know for set context

namespace stapin
{   
    /**
     * @brief RPC CPU Context representation
     * 
     */
    struct Rpc_context
    {
        // TBD
    };
    struct alignas(1) Rpc_section
    {
        alignas(1) uint16_t sectionNameOffset; //!< Section name offset in a section names buffer
        alignas(1) uint64_t sectionAddress;
        alignas(1) unsigned sectionSize;
        alignas(1) uint64_t fileOffset;
        alignas(1) int8_t mmFlags;
        alignas(1) int8_t isBSS;
    };

    /**
     * @brief RPC Source location representation
     * 
     */
    struct alignas(1) Rpc_source_location
    {
        alignas(1) uint16_t sourceFileOffset; //!< Source file name offset in a names buffer
        alignas(1) uint64_t startAddress;
        alignas(1) int32_t startLine;
        alignas(1) int32_t startColumn;
        alignas(1) uint64_t endAddress;
        alignas(1) int32_t endLine;
        alignas(1) int32_t endColumn;
        alignas(1) uint64_t nextAddress;
        alignas(1) uint64_t nextStmtAddress;
    };

    /**
     * @brief RPC symbol representation
     * 
     */
    struct alignas(1) Rpc_symbol
    {
        alignas(1) uint16_t nameOffset;         //!< Symbol name offset in a names buffer
        alignas(1) uint16_t uniqueIdOffset;     //!< unique Id offset in a unique Ids buffer
        alignas(1) uint16_t typeIdOffset;       //!< unique Id of the type of the symbol in a unique Ids buffer
        alignas(1) uint16_t baseTypeIdOffset;   //!< unique Id of the base type of the symbol in a unique Ids buffer
        alignas(1) unsigned type;
        alignas(1) unsigned size;
        alignas(1) uint64_t memoryOrReg;
        alignas(1) int flags;
    };

    /**
     * @brief RPC expression representation
     *
     */
    struct alignas(1) Rpc_expression
    {
        alignas(1) uint16_t expressionNameOffset;
        alignas(1) uint16_t parentExpressionNameOffset;
        alignas(1) uint16_t typeUniqueIDOffset;
        alignas(1) uint16_t symbolUniqueIDoffset;
        alignas(1) uint16_t dataOffset;
        alignas(1) uint16_t dataSize;
        alignas(1) unsigned type;
        alignas(1) int level;
        alignas(1) bool isValue;
    };

    struct RPC_Partial_Context{
        #if defined(TARGET_IA32E)
        uint8_t RIP[8]; //ip
        uint8_t RBP[8]; 
        uint8_t RSP[8]; 
        uint8_t RAX[8];
        uint8_t RDX[8]; 
        uint8_t RCX[8]; 
        uint8_t RBX[8]; 
        uint8_t RSI[8]; 
        uint8_t RDI[8];  
        uint8_t R8[8];
        uint8_t R9[8];
        uint8_t R10[8];
        uint8_t R11[8];
        uint8_t R12[8];
        uint8_t R13[8];
        uint8_t R14[8];
        uint8_t R15[8];
        uint8_t EFLAGS[4]; 
        uint8_t SEG_ES[2]; 
        uint8_t SEG_CS[2];
        uint8_t SEG_SS[2];
        uint8_t SEG_DS[2];
        uint8_t SEG_FS[2];
        uint8_t SEG_GS[2];
        uint8_t SEG_FS_BASE[8]; 
        uint8_t SEG_GS_BASE[8];
        uint8_t MXCSR[4];
        uint8_t MXCSRMASK[8];
        uint8_t SEG_FPSW[8]; 
        uint8_t SEG_FPCW[8]; 
    #else // not defined(TARGET_IA32E)
        uint8_t EIP[4];
        uint8_t EBP[4];
        uint8_t ESP[4];
        uint8_t EAX[4];
        uint8_t EBX[4];
        uint8_t ECX[4];
        uint8_t EDX[4];
        uint8_t ESI[4];
        uint8_t EDI[4];
        uint8_t EFLAGS[2]; 
        uint8_t SEG_ES[2];
        uint8_t SEG_CS[2];
        uint8_t SEG_SS[2];
        uint8_t SEG_DS[2];
        uint8_t SEG_FS[2];
        uint8_t SEG_GS[2];
        uint8_t MXCSR[2];
        uint8_t MXCSRMASK[8];
    #endif // not defined(TARGET_IA32E) 
    };

    struct RPC_Full_Context{
        RPC_Partial_Context partial_context;
        uint8_t ST0[10]; 
        uint8_t ST1[10];
        uint8_t ST2[10];
        uint8_t ST3[10];
        uint8_t ST4[10];
        uint8_t ST5[10];
        uint8_t ST6[10];
        uint8_t ST7[10];
        uint8_t FPOPCODE[8]; //fop
        uint8_t FPIP_OFF[8]; //fip
        uint8_t FPIP_SEL[8]; //fcs
        uint8_t FPDP_OFF[8]; //foo 
        uint8_t FPDP_SEL[8]; //fos
        uint8_t XMM0[16];         
        uint8_t XMM1[16];
        uint8_t XMM2[16];
        uint8_t XMM3[16];
        uint8_t XMM4[16];
        uint8_t XMM5[16];
        uint8_t XMM6[16];
        uint8_t XMM7[16];
        uint8_t XMM8[16];
        uint8_t XMM9[16];
        uint8_t XMM10[16];
        uint8_t XMM11[16];
        uint8_t XMM12[16];
        uint8_t XMM13[16];
        uint8_t XMM14[16];
        uint8_t XMM15[16];
    };


    

    inline constexpr uint16_t PARTIAL_RPC_CONTEXT_SIZE = sizeof(RPC_Partial_Context);
    inline constexpr uint16_t FULL_RPC_CONTEXT_SIZE = sizeof(RPC_Full_Context);

    inline constexpr t_rpc_id STAPIN_RPCID_BASE                 = RPCID_MIN + 4000;
    inline constexpr t_rpc_id RPCID_STAPIN_INIT                 = STAPIN_RPCID_BASE + 0;
    inline constexpr t_rpc_id RPCID_STAPIN_IMAGE_LOAD           = STAPIN_RPCID_BASE + 1;
    inline constexpr t_rpc_id RPCID_STAPIN_IMAGE_UNLOAD         = STAPIN_RPCID_BASE + 2;
    inline constexpr t_rpc_id RPCID_STAPIN_THREAD_START         = STAPIN_RPCID_BASE + 3;
    inline constexpr t_rpc_id RPCID_STAPIN_THREAD_FINI          = STAPIN_RPCID_BASE + 4;
    inline constexpr t_rpc_id RPCID_STAPIN_FORK_IN_CHILD        = STAPIN_RPCID_BASE + 5;
    inline constexpr t_rpc_id RPCID_STAPIN_SET_CONTEXT          = STAPIN_RPCID_BASE + 6;
    inline constexpr t_rpc_id RPCID_STAPIN_SET_IP               = STAPIN_RPCID_BASE + 7;
    inline constexpr t_rpc_id RPCID_STAPIN_GET_SOURCE_LOCATIONS = STAPIN_RPCID_BASE + 8;
    inline constexpr t_rpc_id RPCID_STAPIN_GET_SYMBOLS_START    = STAPIN_RPCID_BASE + 9;
    inline constexpr t_rpc_id RPCID_STAPIN_GET_SYMBOLS_NEXT     = STAPIN_RPCID_BASE + 10;
    inline constexpr t_rpc_id RPCID_STAPIN_GET_SYMBOLS_END      = STAPIN_RPCID_BASE + 11;
    inline constexpr t_rpc_id RPCID_STAPIN_GET_SYMBOL_BY_ADDR   = STAPIN_RPCID_BASE + 12;
    inline constexpr t_rpc_id RPCID_STAPIN_GET_SYMBOL_BY_REG    = STAPIN_RPCID_BASE + 13;
    inline constexpr t_rpc_id RPCID_STAPIN_GET_SYMBOL_BY_ID     = STAPIN_RPCID_BASE + 14;
    inline constexpr t_rpc_id RPCID_STAPIN_GET_EXPRESSION_START = STAPIN_RPCID_BASE + 15;
    inline constexpr t_rpc_id RPCID_STAPIN_GET_EXPRESSION_NEXT  = STAPIN_RPCID_BASE + 16;
    inline constexpr t_rpc_id RPCID_STAPIN_GET_EXPRESSION_END   = STAPIN_RPCID_BASE + 17;

    /**
     * @brief Specify STAPIN_INIT RPC
     * 
     * The RPC starts the tcf agent and creates a tcf context for the process
     * 
     * @param[in] pid The new process Id
     * 
     * @return true on success, false otherwise.
     * 
     */
    inline constexpr auto STAPIN_INIT_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_INIT,
        pinrt::rscschema::Bool_schema_v,             // Return value - init success (true/false)
        pinrt::rscschema::Int_schema_v< NATIVE_TID > // [in] PID for the current process
        >::schema;

    /**
     * @brief Specify STAPIN_IMAGE_LOAD RPC
     * 
     * The RPC registers an new image in the tracked process memory map
     * 
     * @param[in] pid           The process Id for which to register the image
     * @param[in] imageName     The full path to the image file
     * @param[in] sectionNames  A buffer containing all section names for the sections
     *                          passed in \a sections, separated by \0
     * @param[in] sections      An array of @ref Rpc_section objects
     * @param[in] sectionCount  The number of sections passed in \a sections
     * 
     * @return true if the image was successfully registered, false otherwise.
     */
    inline constexpr auto STAPIN_IMAGE_LOAD_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_IMAGE_LOAD,
        pinrt::rscschema::Bool_schema_v,              // Return value - image registration success (true/false)
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the current process
        pinrt::rscschema::Buffer_schema_v,            // [in] Image file name
        pinrt::rscschema::Buffer_schema_v,            // [in] Section names array separated by \0
        pinrt::rscschema::Buffer_schema_v,            // [in] Rpc_section array
        pinrt::rscschema::Uint_schema_v< uint16_t >   // [in] Section count
        >::schema;

    /**
     * @brief Specify STAPIN_IMAGE_UNLOAD RPC
     * 
     * The RPC unregisters an image registered with STAPIN_IMAGE_LOAD
     * All sections belonging to the image will be removed from the memory map.
     * 
     * @param[in] pid           The process Id for which to unregister the image
     * @param[in] imageName     The full path to the image file
     * 
     * @returns true if the image was successfully unregistered
     * 
     */
    inline constexpr auto STAPIN_IMAGE_UNLOAD_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_IMAGE_UNLOAD,
        pinrt::rscschema::Bool_schema_v,              // Return value - image unload success (true/false)
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the current process
        pinrt::rscschema::Buffer_schema_v             // [in] Image file name
        >::schema;

    /**
     * @brief Specify STAPIN_THREAD_START RPC
     * 
     * This RPC registers a new thread with the tcf agent and creates a tcf context for it.
     * 
     * @param[in] pid   The process Id for which to register the thread
     * @param[in] tid   The thread Id to register. This should be the native OS thread Id
     * @param[in] ctx   The full Rpc_context at thread start
     * 
     * @return true if the thread was successfully registered, false otherwise.
     * 
     */
    inline constexpr auto STAPIN_THREAD_START_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_THREAD_START,
        pinrt::rscschema::Bool_schema_v,              // Return value - thread registration success (true/false)
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the current process
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] Native TID for the new thread
        pinrt::rscschema::Buffer_schema_v             // [in] Context for the new thread
        >::schema;

    /**
     * @brief Specify STAPIN_THREAD_FINI RPC
     * 
     * This RPC unregisters and release resources for a thread registered using STAPIN_THREAD_START.
     * 
     * @param[in] pid   The process Id for which to unregister the thread
     * @param[in] tid   The thread Id to unregister. This should be the native OS thread Id
     * 
     * @return true if the thread was successfully unregistered, false otherwise.
     */
    inline constexpr auto STAPIN_THREAD_FINI_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_THREAD_FINI,
        pinrt::rscschema::Bool_schema_v,              // Return value - thread fini success (true/false)
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the current process
        pinrt::rscschema::Int_schema_v< NATIVE_TID >  // [in] Native TID for the exiting thread
        >::schema;

    /**
     * @brief Specify STAPIN_FORK_IN_CHILD RPC
     * 
     * This RPC is called from a forked child to cleanup the tcf state of the parent
     * and reinitialize it for the child.
     * 
     * @param[in] pid   The process Id of the new child
     * @param[in] ppid  The process of the parent Id
     * @param[in] ctx   The full Rpc_context at fork in child
     * 
     * @return true if child registration succeeded, false otherwise.
     * 
     */
    inline constexpr auto STAPIN_FORK_IN_CHILD_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_FORK_IN_CHILD,
        pinrt::rscschema::Bool_schema_v,              // Return value - fork in child success (true/false)
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the child process
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the parent process
        pinrt::rscschema::Buffer_schema_v             // [in] Context for the new process
        >::schema;

    /**
     * @brief Specify STAPIN_SET_CONTEXT RPC
     * 
     * This RPC is called to set the CPU context of a thread
     * 
     * @param[in] pid       The process Id for which to set the context
     * @param[in] tid       The thread Id for which to set the context. This should be the native OS thread Id
     * @param[in] ctx       The Rpc_context representing the CPU context
     * @param[in] ctxSize   The size of the context. This argument is also used to determine if this is a full
     *                      or partial context (including floating point state and registers or not).
     * 
     * @return true if setting the CPU context was successful, false otherwise.
     */
    inline constexpr auto STAPIN_SET_CONTEXT_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_SET_CONTEXT,
        pinrt::rscschema::Bool_schema_v,              // Return value - set context success (true/false)
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the current process
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] Native TID for the current thread
        pinrt::rscschema::Buffer_schema_v,            // [in] Context for the current thread
        pinrt::rscschema::Uint_schema_v< uint16_t >   // [in] Context size (to determine if this is a full or partial context)
        >::schema;

    /**
     * @brief Specify STAPIN_SET_IP RPC
     * 
     * This RPC is used to update only the IP of the CPU context for the given thread.
     * 
     * @param[in] pid       The process Id for which to set the context
     * @param[in] tid       The thread Id for which to set the context. This should be the native OS thread Id
     * @param[in] ip        The new instruction pointer
     * 
     * @return true if setting the IP was successful, false otherwise.
     */
    inline constexpr auto STAPIN_SET_IP_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_SET_IP,
        pinrt::rscschema::Bool_schema_v,              // Return value - set IP success (true/false)
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the current process
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] Native TID for the current thread
        pinrt::rscschema::Uint_schema_v< ADDRINT >    // [in] The new IP
        >::schema;

    /**
     * @brief Specify STAPIN_GET_SOURCE_LOCATIONS RPC
     * 
     * This RPC is used to get the source locations for a binary code range.
     * 
     * @param[in]       pid              The process Id for which to get the sources location
     * @param[in]       startAddress     The start of the address range to get source location information for.
     * @param[in]       endAddress       The end of the address range to get source location information for.
     * @param[out]      sourceFileNames  A buffer containing the names of the source files referenced by the
     *                                   entries in the \a locations array. The names are separated by \0.
     * @param[in,out]   sourceNamesSize  The size of the \a sourceFileNames buffer. On return this argument will
     *                                   hold the actual size of the data stored within the buffer. If the buffer
     *                                   size is not enough to hold all the file names then the RPC will fail.
     * @param[out]      locations        An array of Rpc_source_location objects.
     * @param[in,out]   locCount         The maximum number of entries that can be placed in \a locations.
     *                                   STAPIN_GET_SOURCE_LOCATIONS will return upto locCount entries even
     *                                   if there are more locations that belong the address range.
     * 
     * @return The number of entries placed in \a locations. This number will be less or equal to \a locCount.
     * 
     */
    inline constexpr auto STAPIN_GET_SOURCE_LOCATIONS_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_GET_SOURCE_LOCATIONS,
        pinrt::rscschema::Uint_schema_v< size_t >,    // Return value - Number of locations returned
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the current process
        pinrt::rscschema::Uint_schema_v< ADDRINT >,   // [in] Start of binary code range
        pinrt::rscschema::Uint_schema_v< ADDRINT >,   // [in] End of binary code range
        pinrt::rscschema::Buffer_schema_v,            // [out] Source file names separated by \0
        pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the source files buffer
        pinrt::rscschema::Buffer_schema_v,            // [out] Array of Rpc_source_location objects
        pinrt::rscschema::Uint_schema_v< uint16_t >   // [in] Rpc_source_location count
        >::schema;

    /**
     * @brief Specify GET_SYMBOLS_START RPC
     * 
     * This RPC creates an iterator that can be used to go over symbols visible in a scope.
     * This API will also receive the first batch of symbols.
     * 
     * @param[in]       pid             The process Id for which to query symbols
     * @param[in]       tid             The thread Id for which to query symbols. This should be the native OS thread Id
     * @param[in]       parent          The unique Id of the parent symbol scope. If this argument is empty then no containing
     *                                  symbol scope is used.
     * @param[in]       symName         A symbol name to search for in the current scope. If this argument is empty then all
     *                                  symbols in scope are returned.
     * @param[out]      names           An array of symbol names referenced in entries of the \a symbols array.
     * @param[in,out]   namesSize       Size of \a names buffer. On return this argument will
     *                                  hold the actual size of the data stored within the buffer. If the buffer
     *                                  size is not enough to hold all the symbol names then the RPC will fail.
     * @param[out]      ids             An array of symbol unique Ids referenced in entries of the \a symbols array.
     * @param[in,out]   idSize          Size of \a ids buffer. On return this argument will
     *                                  hold the actual size of the data stored within the buffer. If the buffer
     *                                  size is not enough to hold all the symbol unique ids then the RPC will fail.
     * @param[out]      symbols         An array of Rpc_symbol objects.
     * @param[in,out]   symbolsCount    The \a symbols entry count. On return this value contains the actual number
     *                                  of symbols retrieved for the address range. This value can be 0 on both input
     *                                  and output.
     * 
     * @return The Id of the remote iterator. If there was an error then \c 0 is returned. An iterator object is created
     *         even if there are no symbols for the given address range. The iterator should be closed by a call to
     *         STAPIN_GET_SYMBOLS_END.
     *
     */
    inline constexpr auto STAPIN_GET_SYMBOLS_START_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_GET_SYMBOLS_START,
        pinrt::rscschema::Uint_schema_v< uintptr_t >, // Return value - Remote iterator id
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the current process
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] Native TID for the current thread
        pinrt::rscschema::Buffer_schema_v,            // [in] Parent symbol unique id
        pinrt::rscschema::Buffer_schema_v,            // [in] Name of symbol to search
        pinrt::rscschema::Buffer_schema_v,            // [out] Symbol names separated by \0
        pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the symbol names buffer
        pinrt::rscschema::Buffer_schema_v,            // [out] Unique Ids separated by \0
        pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the ids buffer buffer
        pinrt::rscschema::Buffer_schema_v,            // [out] Array of Rpc_symbol structures
        pinrt::rscschema::Uint_schema_v< uint16_t >   // [in,out] Rpc_symbol count
        >::schema;

    /**
     * @brief Specify STAPIN_GET_SYMBOLS_NEXT RPC
     * 
     * This RPC is used to get the next symbols batch from an iterator.
     * 
     * @param[in]       iteratorId  The iteratorId for the iterator opened by STAPIN_GET_SYMBOLS_START.
     * @param[out]      names           An array of symbol names referenced in entries of the \a symbols array.
     * @param[in,out]   namesSize       Size of \a names buffer. On return this argument will
     *                                  hold the actual size of the data stored within the buffer. If the buffer
     *                                  size is not enough to hold all the symbol names then the RPC will fail.
     * @param[out]      ids             An array of symbol unique Ids referenced in entries of the \a symbols array.
     * @param[in,out]   idSize          Size of \a ids buffer. On return this argument will
     *                                  hold the actual size of the data stored within the buffer. If the buffer
     *                                  size is not enough to hold all the symbol unique ids then the RPC will fail.
     * @param[out]      symbols         An array of Rpc_symbol objects.
     * @param[in,out ]  symbolsCount    The \a symbols entry count. On return this value contains the actual number
     *                                  of symbols retrieved for the address range. This value can be 0 on both input
     *                                  and output.
     * 
     * @return true if there are more symbols yet to be retrieved.
     * @return false if an error ocurred or if this was the last batch.
     *         After this function returns false the iterator cannot be used again and should be closed
     *         with STAPIN_GET_SYMBOLS_END.
     * 
     */
    inline constexpr auto STAPIN_GET_SYMBOLS_NEXT_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_GET_SYMBOLS_NEXT,
        pinrt::rscschema::Bool_schema_v,              // Return value - Symbols returned or false if the iterator reached the end
        pinrt::rscschema::Uint_schema_v< uintptr_t >, // [in] The remote iterator Id
        pinrt::rscschema::Buffer_schema_v,            // [out] Symbol names separated by \0
        pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the symbol names buffer
        pinrt::rscschema::Buffer_schema_v,            // [out] Unique Ids separated by \0
        pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the ids buffer buffer
        pinrt::rscschema::Buffer_schema_v,            // [out] Array of Rpc_symbol structures
        pinrt::rscschema::Uint_schema_v< uint16_t >   // [in,out] Rpc_symbol count
        >::schema;

    /**
     * @brief Specify STAPIN_GET_SYMBOLS_END RPC
     * 
     * This RPC is used to close a symbols iterator opened by STAPIN_GET_SYMBOLS_START.
     * 
     * @param[in] iteratorId  The iteratorId for the iterator opened by STAPIN_GET_SYMBOLS_START.
     * 
     */
    inline constexpr auto STAPIN_GET_SYMBOLS_END_SCHEMA =
        pinrt::rscschema::RPC_message_schema_wrapper< RPCID_STAPIN_GET_SYMBOLS_END,
                                                      pinrt::rscschema::Void_schema_v,             // Return value (void)
                                                      pinrt::rscschema::Uint_schema_v< uintptr_t > // [in] The remote iterator Id
                                                      >::schema;

    /**
     * @brief Specify STAPIN_GET_SYMBOL_BY_ADDR RPC
     * 
     * This RPC is used to get symbol information from the address of a symbol.
     * 
     * @param[in]       pid             The process Id for which to query symbols
     * @param[in]       tid             The thread Id for which to query symbols. This should be the native OS thread Id
     * @param[in]       address         The address of the symbol
     * @param[out]      name            The symbol name.
     * @param[in,out]   namesSize       Size of \a name buffer. On return this argument will
     *                                  hold the actual size of the data stored within the buffer. If the buffer
     *                                  size is not enough to hold all the symbol name then the RPC will fail.
     * @param[out]      ids             An array of symbol unique Ids referenced in entries of the \a symbols array.
     * @param[in,out]   idSize          Size of \a ids buffer. On return this argument will
     *                                  hold the actual size of the data stored within the buffer. If the buffer
     *                                  size is not enough to hold all the symbol unique ids then the RPC will fail.
     * @param[out]      symbol          An Rpc_symbol object.
     * 
     */
    inline constexpr auto STAPIN_GET_SYMBOL_BY_ADDR_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_GET_SYMBOL_BY_ADDR,
        pinrt::rscschema::Bool_schema_v,              // Return value - true is a sumbol was found for the address
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the current process
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] Native TID for the current thread
        pinrt::rscschema::Uint_schema_v< ADDRINT >,   // [in] Symbol Address
        pinrt::rscschema::Buffer_schema_v,            // [out] Symbol Name
        pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the symbol name buffer
        pinrt::rscschema::Buffer_schema_v,            // [out] Symbol and symbol type Unique Ids separated by \0
        pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the symbol id buffer
        pinrt::rscschema::Buffer_schema_v             // [out] Rpc_symbol
        >::schema;

    /**
     * @brief Specify STAPIN_GET_SYMBOL_BY_REG RPC
     * 
     * This RPC is used to get symbol information from a register mapped to a symbol.
     * 
     * @param[in]       pid             The process Id for which to query symbols
     * @param[in]       tid             The thread Id for which to query symbols. This should be the native OS thread Id
     * @param[in]       reg             The register id of the symbol
     * @param[out]      name            The symbol name.
     * @param[in,out]   namesSize       Size of \a name buffer. On return this argument will
     *                                  hold the actual size of the data stored within the buffer. If the buffer
     *                                  size is not enough to hold all the symbol name then the RPC will fail.
     * @param[out]      ids             An array of symbol unique Ids referenced in entries of the \a symbols array.
     * @param[in,out]   idSize          Size of \a ids buffer. On return this argument will
     *                                  hold the actual size of the data stored within the buffer. If the buffer
     *                                  size is not enough to hold all the symbol unique ids then the RPC will fail.
     * @param[out]      symbol          An Rpc_symbol object.
     * 
     */
    inline constexpr auto STAPIN_GET_SYMBOL_BY_REG_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_GET_SYMBOL_BY_REG,
        pinrt::rscschema::Bool_schema_v,              // Return value - true is a sumbol was found for the register
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the current process
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] Native TID for the current thread
        pinrt::rscschema::Uint_schema_v< unsigned >,  // [in] Symbol reg index
        pinrt::rscschema::Buffer_schema_v,            // [out] Symbol Name
        pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the symbol name buffer
        pinrt::rscschema::Buffer_schema_v,            // [out] Symbol and symbol type Unique Ids separated by \0
        pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the symbol id buffer
        pinrt::rscschema::Buffer_schema_v             // [out] Rpc_symbol
        >::schema;

    /**
     * @brief Specify STAPIN_GET_SYMBOL_BY_ID RPC
     * 
     * This RPC is used to get symbol information from a symbol unique id.
     * 
     * @param[in]       pid             The process Id for which to query symbols
     * @param[in]       tid             The thread Id for which to query symbols. This should be the native OS thread Id
     * @param[in]       uniqueId        The unique Id of the symbol
     * @param[out]      name            The symbol name.
     * @param[in,out]   namesSize       Size of \a name buffer. On return this argument will
     *                                  hold the actual size of the data stored within the buffer. If the buffer
     *                                  size is not enough to hold all the symbol name then the RPC will fail.
     * @param[out]      ids             An array of symbol unique Ids referenced in entries of the \a symbols array.
     * @param[in,out]   idSize          Size of \a ids buffer. On return this argument will
     *                                  hold the actual size of the data stored within the buffer. If the buffer
     *                                  size is not enough to hold all the symbol unique ids then the RPC will fail.
     * @param[out]      symbol          An Rpc_symbol object.
     * 
     */
    inline constexpr auto STAPIN_GET_SYMBOL_BY_ID_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
        RPCID_STAPIN_GET_SYMBOL_BY_ID,
        pinrt::rscschema::Bool_schema_v,              // Return value - true is a sumbol was found for the register
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the current process
        pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] Native TID for the current thread
        pinrt::rscschema::Buffer_schema_v,            // [in] Symbol unique id
        pinrt::rscschema::Buffer_schema_v,            // [out] Symbol Name
        pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the symbol name buffer
        pinrt::rscschema::Buffer_schema_v,            // [out] Symbol and symbol type Unique Ids separated by \0
        pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the symbol id buffer
        pinrt::rscschema::Buffer_schema_v             // [out] Rpc_symbol
        >::schema;

        /**
     * @brief Specify GET_EXPRESSION_START RPC
     *
     * This RPC creates an iterator that can be used to go over an expression tree result of an expression evaluation request.
     * This API will also receive the first batch of expressions returned from the input expression evaluation.
     *
     * @param[in]       pid             The process Id for which to evaluate the expression.
     * @param[in]       tid             The thread Id for which to evaluate the expression. This should be the native OS thread Id.
     * @param[in]       expr            An expression to evaluate.
     * @param[out]      names           An array of expressions names referenced in entries of the expressions array.
     * expression names: The first name is the expression input. If the expression involves a hierarchical structure, 
     * such as an array or a structure with fields, the names of the child expressions will follow a specific naming convention.
     * @param[in,out]   namesSize       Size of names buffer. On return this argument will
     *                                  hold the actual size of the data stored within the buffer. If the buffer
     *                                  size is not enough to hold all the expressions names then the RPC will fail.
     * @param[out]      expressions     An array of Rpc_expression objects.
     * @param[in,out]   expressionCount    The expressions entry count. On return this value contains the actual number
     *                                  of expressions retrieved. This value can be 0 on both input
     *                                  and output.
     *
     * @return The Id of the remote iterator. If there was an error then 0 is returned. An iterator object is created
     *         even if there are no expressions evaluated. The iterator should be closed by a call to
     *         STAPIN_GET_EXPRESSION_END.
     *
     */
    inline constexpr auto STAPIN_GET_EXPRESSION_START_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
            RPCID_STAPIN_GET_EXPRESSION_START,
            pinrt::rscschema::Uint_schema_v< uintptr_t >, // Return value - Remote iterator id
            pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] PID for the current process
            pinrt::rscschema::Int_schema_v< NATIVE_TID >, // [in] Native TID for the current thread
            pinrt::rscschema::Buffer_schema_v,            // [in] Name of expression to evaluate
            pinrt::rscschema::Buffer_schema_v,            // [out] Expression name and it's children names separated by \0 if there are any.
            pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the expression names buffer
            pinrt::rscschema::Buffer_schema_v,            // [out] Type Unique Ids separated by \0
            pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the ids buffer buffer
            pinrt::rscschema::Buffer_schema_v,            // [out] Array of Rpc_expression structures
            pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Rpc_expression count
            pinrt::rscschema::Buffer_schema_v,            // [in,out] Data array (expression value).
            pinrt::rscschema::Uint_schema_v< uint16_t >   // [in,out] Size of the data array.
            >::schema;


    /**
     * @brief Specify STAPIN_GET_EXPRESSION_NEXT RPC
     *
     * This RPC is used to get the next expression batch from an iterator.
     *
     * @param[in]       iteratorId      The iteratorId for the iterator opened by STAPIN_GET_EXPRESSION_START.
     * @param[out]      names           An array of expressions names referenced in entries of the expressions array.
     * @param[in,out]   namesSize       Size of names buffer. On return this argument will
     *                                  hold the actual size of the data stored within the buffer. If the buffer
     *                                  size is not enough to hold all the expression names then the RPC will fail.
     * @param[out]      ids             An array of expressions type unique Ids referenced in entries of the \a expressions array.
     * @param[in,out]   idSize          Size of \a ids buffer. On return this argument will
     *                                  hold the actual size of the data stored within the buffer. If the buffer
     *                                  size is not enough to hold all the symbol unique ids then the RPC will fail.
     * @param[out]      expressions     An array of Rpc_expression objects.
     * @param[in,out]   expressionsCount    The expressions entry count. On return this value contains the actual number
     *                                  of expressions retrieved. This value can be 0 on both input
     *                                  and output.
     *
     * @return true if there are more expressions yet to be retrieved.
     * @return false if an error ocurred or if this was the last batch.
     *         After this function returns false the iterator cannot be used again and should be closed
     *         with STAPIN_GET_EXPRESSION_END.
     *
     */
    inline constexpr auto STAPIN_GET_EXPRESSION_NEXT_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
            RPCID_STAPIN_GET_EXPRESSION_NEXT,
            pinrt::rscschema::Bool_schema_v,              // Return value - Expressions returned or false if the iterator reached the end
            pinrt::rscschema::Uint_schema_v< uintptr_t >, // [in] The remote iterator Id
            pinrt::rscschema::Buffer_schema_v,            // [out] Expression name and it's children names separated by \0 if there are any.
            pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the expression names buffer
            pinrt::rscschema::Buffer_schema_v,            // [out] Type Unique Ids separated by \0
            pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Size of the ids buffer buffer
            pinrt::rscschema::Buffer_schema_v,            // [out] Array of Rpc_expression structures
            pinrt::rscschema::Uint_schema_v< uint16_t >,  // [in,out] Rpc_expression count
            pinrt::rscschema::Buffer_schema_v,            // [in,out] Data array (expression value).
            pinrt::rscschema::Uint_schema_v< uint16_t >   // [in,out] Size of the data array.
            >::schema;

    /**
     * @brief Specify STAPIN_GET_EXPRESSION_END RPC
     *
     * This RPC is used to close an expression iterator opened by STAPIN_GET_EXPRESSION_START.
     *
     * @param[in] iteratorId  The iteratorId for the iterator opened by STAPIN_GET_EXPRESSION_START.
     *
     */
    inline constexpr auto STAPIN_GET_EXPRESSION_END_SCHEMA = pinrt::rscschema::RPC_message_schema_wrapper<
            RPCID_STAPIN_GET_EXPRESSION_END,
            pinrt::rscschema::Void_schema_v,             // Return value (void)
            pinrt::rscschema::Uint_schema_v< uintptr_t > // [in] The remote iterator Id
            >::schema;


} // namespace stapin

#endif // _STAPIN_SCHEMAS_H_
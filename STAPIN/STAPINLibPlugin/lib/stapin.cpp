/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <stapin.h>

#include <stapin_schemas.h>
#include <buffer_pool.h>

#include <cstring>
#include <iostream>
#include <list>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <unistd.h>

/**
 * @brief A custom iterator structure for handling a list of values.
 * @tparam VALUE_TYPE The type of the values stored in the iterator's list.
 */
template <typename VALUE_TYPE>
struct stapin::STAPIN_iterator {
    using t_value_list = std::list<VALUE_TYPE>; //!< Type alias for the list of values.
    uintptr_t ID; //!< Unique identifier for the iterator.
    typename t_value_list::iterator iter; //!< Iterator pointing to the current position in the list.
    t_value_list values; //!< List of values managed by the iterator.
};




namespace{
    
    constexpr uint16_t SYMBOL_COUNT = 300;
    constexpr uint16_t UNIQUE_IDS_BUFFER_SIZE = 13000;
    constexpr uint16_t SYMBOLS_NAMES_BUFFER_SIZE = 13000;
    constexpr uint16_t SOURCE_FILE_NAMES_BUFFER_SIZE = 13000;
    constexpr uint16_t EXPRESSION_NAMES_BUFFER_SIZE = 13000;
    constexpr uint16_t SINGLE_ID_SIZE = 512;
    constexpr uint16_t EXPRESSION_COUNT = 300;
    constexpr uint16_t MAX_DATA_SIZE = 64;
    constexpr uint16_t BUFFER_POOL_INIT_CAPACITY = 24;

    constexpr uint16_t NO_OFFSET = -1;

    constexpr char NULL_TERMINATOR = '\0';

    constexpr int FLAG_R =  1;
    constexpr int FLAG_W =  2;
    constexpr int FLAG_X =  4;

    /*
    * Global buffer pool.
    */
    bufferpool::Buffer_pool::Buffer_pool_ptr bufferPool;

    std::mutex namesAndIDsMutex;

    std::unordered_set<std::string> namesAndIDs;

    NATIVE_TID _pid;

    /**
     * @brief Structure to hold buffers for RPC calls.
     * @tparam RPC_BUFFER_TYPE The type of the RPC buffer.
     */
    template <typename RPC_BUFFER_TYPE>
    struct Buffers_for_rpc {
        std::unique_ptr<bufferpool::Buffer, bufferpool::Buffer_deleter> namesBuffer; //!< Buffer for names.
        std::unique_ptr<bufferpool::Buffer, bufferpool::Buffer_deleter> uniqueIDSBuffer; //!< Buffer for unique IDs.
        std::unique_ptr<bufferpool::Buffer, bufferpool::Buffer_deleter> rpcValuesBuffer; //!< Buffer for RPC values.
        std::unique_ptr<bufferpool::Buffer, bufferpool::Buffer_deleter> expressionsDataBuffer; //!< Buffer for RPC values.
        uint16_t count; //!< Number of items in the buffers.
        bool success; //!< Flag indicating if the buffer creation was successful.
    
        Buffers_for_rpc(): 
            namesBuffer(bufferPool->get_buffer()),
            uniqueIDSBuffer(bufferPool->get_buffer()),
            rpcValuesBuffer(bufferPool->get_buffer()),
            expressionsDataBuffer(bufferPool->get_null_buffer()),
            count(0),
            success(true) 
        {
            //If the pool is empty then try again until a buffer is acquired
            while(nullptr == namesBuffer)
            {
                namesBuffer = bufferPool->get_buffer();
            }
            while(nullptr == uniqueIDSBuffer)
            {
                uniqueIDSBuffer = bufferPool->get_buffer();
            }
            while(nullptr == rpcValuesBuffer)
            {
                rpcValuesBuffer = bufferPool->get_buffer();
            }
            if constexpr (std::is_same_v<RPC_BUFFER_TYPE, stapin::Rpc_expression>) {
                while(nullptr == expressionsDataBuffer)
                {
                    expressionsDataBuffer = bufferPool->get_buffer();
                }
            }
        }
    };

    /**
     * @brief Template structure to define sizes for different types and conditions.
     * @tparam TYPE The type for which sizes are defined.
     * @tparam SINGLE Boolean indicating if a single item is considered.
     */
    template<typename TYPE, bool SINGLE = false>
    struct Sizes;

    /**
     * @brief Specialization of Sizes for stapin::Rpc_symbol.
     * @tparam SINGLE Boolean indicating if a single item is considered.
     */
    template<bool SINGLE>
    struct Sizes<stapin::Rpc_symbol, SINGLE> {
        static constexpr size_t NAMES_BUFFER_SIZE = SYMBOLS_NAMES_BUFFER_SIZE; //!< Size of the names buffer.
        static constexpr size_t UNIQUEIDS_BUFFER_SIZE = UNIQUE_IDS_BUFFER_SIZE; //!< Size of the unique IDs buffer.
        static constexpr size_t RPC_BUFFER_SIZE = SINGLE ? sizeof(stapin::Rpc_symbol) : SYMBOL_COUNT*sizeof(stapin::Rpc_symbol); //!< Size of the RPC buffer.
        static constexpr size_t RPC_COUNT =  SINGLE ? 1: SYMBOL_COUNT; //!< Count of RPC items.
    };

    /**
     * @brief Specialization of Sizes for stapin::Rpc_expression.
     * @tparam SINGLE Boolean indicating if a single item is considered.
     */
    template<bool SINGLE>
    struct Sizes<stapin::Rpc_expression, SINGLE> {
        static constexpr size_t NAMES_BUFFER_SIZE = EXPRESSION_NAMES_BUFFER_SIZE; //!< Size of the names buffer.
        static constexpr size_t UNIQUEIDS_BUFFER_SIZE = UNIQUE_IDS_BUFFER_SIZE; //!< Size of the unique IDs buffer.
        static constexpr size_t RPC_BUFFER_SIZE = EXPRESSION_COUNT * (sizeof(stapin::Rpc_expression) + stapin::EXPRESSION_DATA_BYTES); //!< Size of the RPC buffer. (plus extra for data).
        static constexpr size_t RPC_COUNT = EXPRESSION_COUNT; //!< Count of RPC items.
        static constexpr size_t DATA_BUFFER_SIZE = MAX_DATA_SIZE * EXPRESSION_COUNT;
    };

    /**
    * @brief Creates the buffers for rpc calls.
    * @tparam RPC_BUFFER_TYPE type of the rpc buffer items. 
    * @tparam SINGLE Boolean indicating if a single item is considered.
    * @param singleSymbol Boolean indicating if we need a single symbol (getting the symbol by query).
    * @return Buffers_for_rpc with the fields full and flags to know if the function was a success.
    */
    template <typename RPC_BUFFER_TYPE, bool SINGLE = false >
    Buffers_for_rpc<RPC_BUFFER_TYPE> create_rpc_buffers() 
    {
        Buffers_for_rpc<RPC_BUFFER_TYPE> buffers;
        buffers.count = Sizes<RPC_BUFFER_TYPE, SINGLE>::RPC_COUNT;
        return buffers;
    }

    /**
    * @brief Notifies about failure of doRpc funciton.
    * @param stapinMessageFailed the message that need to show.
    */
    void notify_pin_do_rpc_fail(const std::string& stapinMessageFailed){
        std::cout << "PIN_DoRPC failed in the message ->  " << stapinMessageFailed << std::endl;
    }

    /**
    * @brief Retrives IP value from context.
    * @param ctx PIN context.
    * @return uintptr_t which is the value of the IP.
    */
    uintptr_t get_IP_from_pin_context(const CONTEXT* ctx){
        LEVEL_BASE::REG IP_reg;
        #if defined(TARGET_IA32E)
        IP_reg = LEVEL_BASE::REG_RIP;
        #else 
        IP_reg = LEVEL_BASE::REG_EIP;
        #endif
        return PIN_GetContextReg(ctx, IP_reg);    
    }
     
    const LEVEL_BASE::REG INDEX_TO_REG_MAP[] {
        LEVEL_BASE::REG_NONE,
        #if defined(TARGET_IA32E)
        LEVEL_BASE::REG_RIP,
        LEVEL_BASE::REG_RBP,
        LEVEL_BASE::REG_RSP,
        LEVEL_BASE::REG_RAX,
        LEVEL_BASE::REG_RDX,
        LEVEL_BASE::REG_RCX,
        LEVEL_BASE::REG_RBX,
        LEVEL_BASE::REG_RSI,
        LEVEL_BASE::REG_RDI,
        LEVEL_BASE::REG_R8,
        LEVEL_BASE::REG_R9,
        LEVEL_BASE::REG_R10,
        LEVEL_BASE::REG_R11,
        LEVEL_BASE::REG_R12,
        LEVEL_BASE::REG_R13,
        LEVEL_BASE::REG_R14,
        LEVEL_BASE::REG_R15,
        LEVEL_BASE::REG_EFLAGS,
        LEVEL_BASE::REG_SEG_ES,
        LEVEL_BASE::REG_SEG_CS,
        LEVEL_BASE::REG_SEG_SS,
        LEVEL_BASE::REG_SEG_DS,
        LEVEL_BASE::REG_SEG_FS,
        LEVEL_BASE::REG_SEG_GS,
        LEVEL_BASE::REG_SEG_FS_BASE,
        LEVEL_BASE::REG_SEG_GS_BASE,
        LEVEL_BASE::REG_MXCSR,
        LEVEL_BASE::REG_MXCSRMASK,
        LEVEL_BASE::REG_ST0,
        LEVEL_BASE::REG_ST1,
        LEVEL_BASE::REG_ST2,
        LEVEL_BASE::REG_ST3,
        LEVEL_BASE::REG_ST4,
        LEVEL_BASE::REG_ST5,
        LEVEL_BASE::REG_ST6,
        LEVEL_BASE::REG_ST7,
        LEVEL_BASE::REG_FPOPCODE,
        LEVEL_BASE::REG_FPIP_OFF,
        LEVEL_BASE::REG_FPIP_SEL,
        LEVEL_BASE::REG_FPDP_OFF,
        LEVEL_BASE::REG_FPDP_SEL,
        LEVEL_BASE::REG_XMM0,
        LEVEL_BASE::REG_XMM1,
        LEVEL_BASE::REG_XMM2,
        LEVEL_BASE::REG_XMM3,
        LEVEL_BASE::REG_XMM4,
        LEVEL_BASE::REG_XMM5,
        LEVEL_BASE::REG_XMM6,
        LEVEL_BASE::REG_XMM7,
        LEVEL_BASE::REG_XMM8,
        LEVEL_BASE::REG_XMM9,
        LEVEL_BASE::REG_XMM10,
        LEVEL_BASE::REG_XMM11,
        LEVEL_BASE::REG_XMM12,
        LEVEL_BASE::REG_XMM13,
        LEVEL_BASE::REG_XMM14,
        LEVEL_BASE::REG_XMM15,
        LEVEL_BASE::REG_FPSW,
        LEVEL_BASE::REG_FPCW
        #else
        LEVEL_BASE::REG_EIP,
        LEVEL_BASE::REG_EBP,
        LEVEL_BASE::REG_ESP,
        LEVEL_BASE::REG_EAX,
        LEVEL_BASE::REG_EBX,
        LEVEL_BASE::REG_ECX,
        LEVEL_BASE::REG_EDX,
        LEVEL_BASE::REG_EBP,
        LEVEL_BASE::REG_ESI,
        LEVEL_BASE::REG_EDI,
        LEVEL_BASE::REG_EFLAGS,
        LEVEL_BASE::REG_SEG_ES,
        LEVEL_BASE::REG_SEG_CS,
        LEVEL_BASE::REG_SEG_SS,
        LEVEL_BASE::REG_SEG_DS,
        LEVEL_BASE::REG_SEG_FS,
        LEVEL_BASE::REG_SEG_GS,
        LEVEL_BASE::REG_MXCSR,
        LEVEL_BASE::REG_MXCSRMASK,
        LEVEL_BASE::REG_ST0,
        LEVEL_BASE::REG_ST1,
        LEVEL_BASE::REG_ST2,
        LEVEL_BASE::REG_ST3,
        LEVEL_BASE::REG_ST4,
        LEVEL_BASE::REG_ST5,
        LEVEL_BASE::REG_ST6,
        LEVEL_BASE::REG_ST7,
        LEVEL_BASE::REG_FPOPCODE,
        LEVEL_BASE::REG_FPIP_OFF,
        LEVEL_BASE::REG_FPIP_SEL,
        LEVEL_BASE::REG_FPDP_OFF,
        LEVEL_BASE::REG_FPDP_SEL,
        LEVEL_BASE::REG_XMM0,
        LEVEL_BASE::REG_XMM1,
        LEVEL_BASE::REG_XMM2,
        LEVEL_BASE::REG_XMM3,
        LEVEL_BASE::REG_XMM4,
        LEVEL_BASE::REG_XMM5,
        LEVEL_BASE::REG_XMM6,
        LEVEL_BASE::REG_XMM7,
        LEVEL_BASE::REG_XMM8,
        LEVEL_BASE::REG_XMM9,
        LEVEL_BASE::REG_XMM10,
        LEVEL_BASE::REG_XMM11,
        LEVEL_BASE::REG_XMM12,
        LEVEL_BASE::REG_XMM13,
        LEVEL_BASE::REG_XMM14,
        LEVEL_BASE::REG_XMM15
        #endif
    };

    /**
    * @brief filling the register values that are in the partial context .
    * @param ctx PIN context.
    * @param ctx partialContext the partial context to fill.
    */
    void fill_partial_context(const CONTEXT* ctx, stapin::RPC_Partial_Context& partialContext)
    {
        #if defined(TARGET_IA32E)
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_RSP, partialContext.RSP);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_RBP, partialContext.RBP);
        PIN_GetContextRegval(ctx,  LEVEL_BASE::REG_RIP,partialContext.RIP);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_RAX, partialContext.RAX);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_RDX, partialContext.RDX);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_RCX, partialContext.RCX);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_RBX, partialContext.RBX);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_RSI, partialContext.RSI);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_RDI, partialContext.RDI);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_R8, partialContext.R8);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_R9, partialContext.R9);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_R10, partialContext.R10);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_R11, partialContext.R11);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_R12, partialContext.R12);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_R13, partialContext.R13);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_R14, partialContext.R14);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_R15, partialContext.R15);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_EFLAGS, partialContext.EFLAGS);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_ES, partialContext.SEG_ES);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_CS, partialContext.SEG_CS);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_SS, partialContext.SEG_SS);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_DS, partialContext.SEG_DS);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_FS, partialContext.SEG_FS);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_GS, partialContext.SEG_GS);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_FS_BASE, partialContext.SEG_FS_BASE);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_GS_BASE, partialContext.SEG_GS_BASE);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_MXCSR, partialContext.MXCSR);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_MXCSRMASK, partialContext.MXCSRMASK);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_FPSW, partialContext.SEG_FPSW);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_FPCW, partialContext.SEG_FPCW);
        #else // not defined(TARGET_IA32E)
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_ESP, partialContext.ESP);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_EBP, partialContext.EBP);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_EIP, partialContext.EIP);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_EAX, partialContext.EAX);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_EBX, partialContext.EBX);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_ECX, partialContext.ECX);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_EDX, partialContext.EDX);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_ESP, partialContext.ESP);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_EBP, partialContext.EBP);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_ESI, partialContext.ESI);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_EDI, partialContext.EDI);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_EIP, partialContext.EIP);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_EFLAGS, partialContext.EFLAGS);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_ES, partialContext.SEG_ES);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_CS, partialContext.SEG_CS);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_SS, partialContext.SEG_SS);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_DS, partialContext.SEG_DS);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_FS, partialContext.SEG_FS);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_SEG_GS, partialContext.SEG_GS);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_MXCSR, partialContext.MXCSR);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_MXCSRMASK, partialContext.MXCSRMASK);
        #endif // not defined(TARGET_IA32E)
    }

    /**
    * @brief filling the register values that are in the full context.
    * @param ctx PIN context.
    * @param ctx fullContext the full context to fill.
    */
    void fill_context(const CONTEXT* ctx, stapin::RPC_Full_Context& fullContext){
        fill_partial_context(ctx, fullContext.partial_context);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_ST0, fullContext.ST0);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_ST1, fullContext.ST1);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_ST2, fullContext.ST2);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_ST3, fullContext.ST3);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_ST4, fullContext.ST4);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_ST5, fullContext.ST5);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_ST6, fullContext.ST6);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_ST7, fullContext.ST7);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_FPOPCODE, fullContext.FPOPCODE);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_FPIP_OFF, fullContext.FPIP_OFF);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_FPIP_SEL , fullContext.FPIP_SEL);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_FPDP_OFF, fullContext.FPDP_OFF);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_FPDP_SEL, fullContext.FPDP_SEL);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM0, fullContext.XMM0);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM1, fullContext.XMM1);   
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM2, fullContext.XMM2);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM3, fullContext.XMM3);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM4, fullContext.XMM4);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM5, fullContext.XMM5);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM6, fullContext.XMM6);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM7, fullContext.XMM7);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM8, fullContext.XMM8);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM9, fullContext.XMM9);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM10, fullContext.XMM10);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM11, fullContext.XMM11);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM12, fullContext.XMM12);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM13, fullContext.XMM13);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM14, fullContext.XMM14);
        PIN_GetContextRegval(ctx, LEVEL_BASE::REG_XMM15, fullContext.XMM15);
    }

    /**
    * @brief find the index of a reg in the INDEX_TO_REG_MAP.
    * @param reg register to find the index of.
    * @return int32_t the result index or 0 if no matching reg was found in INDEX_TO_REG_MAP.
    */
    int32_t find_index_of_reg(LEVEL_BASE::REG reg)
    {
        int32_t  i = 1;
        for(auto c_reg: INDEX_TO_REG_MAP)
        {
            if(c_reg == reg)
            {
                return i;
            }
            i++;
        }
        return 0;
    }

    /**
    * @brief Inserting string (name or ID) to the namesAndIDs db. 
    * @param buffer buffer to take the string from.
    * @param offset offset of the string in the buffer.
    * @return const char* that is the string that was inserted or empty string if nothing was inserted.
    */
    const char* insert_string_to_db(const char buffer[], uint16_t offset)
    {
        return (NO_OFFSET != offset) ? (*namesAndIDs.insert(buffer + offset).first).c_str() : "";
    }

    /**
     * @brief Filling a stapin::Symbol attributes from a given rpc symbol attributes and return the newly created symbol.
     * @param buffers Buffers containing buffers results from do rpc query.
     * @param offset Offset to the specific rpc symbol in the buffer.
     * @param namesBufferSize Size of the buffer containing symbol names.
     * @param uniqueIDsBufferSize Size of the buffer containing unique IDs.
     * @return stapin::Symbol The newly created symbol.
     */
    stapin::Symbol process_symbol(const Buffers_for_rpc<stapin::Rpc_symbol>& buffers, 
                                size_t index,
                                uint16_t namesBufferSize,
                                uint16_t uniqueIDsBufferSize)
    {
        auto rpcSymbol = (stapin::Rpc_symbol*)(buffers.rpcValuesBuffer.get()->data + index*sizeof(stapin::Rpc_symbol));
        ASSERTX (NO_OFFSET == rpcSymbol->nameOffset || rpcSymbol->nameOffset < namesBufferSize);
        ASSERTX (NO_OFFSET == rpcSymbol->uniqueIdOffset || rpcSymbol->uniqueIdOffset < uniqueIDsBufferSize);
        ASSERTX (NO_OFFSET == rpcSymbol->typeIdOffset || rpcSymbol->typeIdOffset < uniqueIDsBufferSize);
        ASSERTX (NO_OFFSET == rpcSymbol->baseTypeIdOffset || rpcSymbol->baseTypeIdOffset < uniqueIDsBufferSize);
        auto currSymbol = stapin::Symbol {0}; 
        {
            std::lock_guard<std::mutex> lock(namesAndIDsMutex);
            auto uniqueIDSBuffer = reinterpret_cast<const char*>(buffers.uniqueIDSBuffer.get()->data);
            currSymbol.name = insert_string_to_db(reinterpret_cast<const char*>(buffers.namesBuffer.get()->data), rpcSymbol->nameOffset);
            currSymbol.uniqueId = insert_string_to_db(uniqueIDSBuffer, rpcSymbol->uniqueIdOffset);
            currSymbol.typeUniqueId = insert_string_to_db(uniqueIDSBuffer, rpcSymbol->typeIdOffset);
            currSymbol.baseTypeUniqueId = insert_string_to_db(uniqueIDSBuffer, rpcSymbol->baseTypeIdOffset);
        }
        currSymbol.type = static_cast<stapin::E_symbol_type>(rpcSymbol->type);
        currSymbol.size = rpcSymbol->size;
        currSymbol.memory = rpcSymbol->memoryOrReg;
        currSymbol.flags = static_cast<stapin::E_symbol_flags>(rpcSymbol->flags);
        if (stapin::E_symbol_flags::VALUE_IN_REG == currSymbol.flags) 
        {
            currSymbol.reg = INDEX_TO_REG_MAP[rpcSymbol->memoryOrReg];
        }
        return currSymbol;
    }

    /**
     * @brief Creating symbols ('buffers.count' number) and inserting them into the iterator.
     * @param buffers Buffers containing buffers results from do rpc query.
     * @param resultSymbolIterator Iterator to insert symbols to.
     * @param namesBufferSize Size of the buffer containing symbol names.
     * @param uniqueIDsBufferSize Size of the buffer containing unique IDs.
     */
    void process_symbols(const Buffers_for_rpc<stapin::Rpc_symbol>& buffers, 
                         stapin::Symbol_iterator* resultSymbolIterator,
                         uint16_t namesBufferSize,
                         uint16_t uniqueIDsBufferSize) 
    {
        if(0 == buffers.count)
        {
            return;
        }
        size_t startIndex = 0;
        if(resultSymbolIterator->values.end() == resultSymbolIterator->iter)
        {
            auto curSymbol = process_symbol(buffers, startIndex , namesBufferSize, uniqueIDsBufferSize);
            resultSymbolIterator->iter = resultSymbolIterator->values.insert(resultSymbolIterator->iter, curSymbol);
            ++startIndex;
        }
        for (size_t i = startIndex; i < buffers.count; ++i)
        {
            auto curSymbol = process_symbol(buffers, i , namesBufferSize, uniqueIDsBufferSize);
            resultSymbolIterator->values.push_back(curSymbol);    
        }
    }

    /**
     * @brief Filling a stapin::Expression attributes from a given rpc expression attributes and return the newly created expression.
     * @param buffers Buffers containing buffers results from do rpc query.
     * @param namesBufferSize Size of the buffer containing expression names.
     * @param uniqueIDsBufferSize Size of the buffer containing unique IDs.
     * @param offset Offset to the specific rpc expression in the buffer.
     * @return stapin::Expression The newly created expression.
     */
    stapin::Expression process_expression(const Buffers_for_rpc<stapin::Rpc_expression>& buffers, 
                                        uint16_t namesBufferSize,
                                        uint16_t uniqueIDsBufferSize,
                                        uint16_t dataBufferSize,
                                        size_t& offset)
    {
        auto rpcExpression = (stapin::Rpc_expression*)(buffers.rpcValuesBuffer.get()->data + offset);
        ASSERTX (NO_OFFSET == rpcExpression->expressionNameOffset || rpcExpression->expressionNameOffset < namesBufferSize);
        ASSERTX (NO_OFFSET == rpcExpression->parentExpressionNameOffset || rpcExpression->parentExpressionNameOffset < namesBufferSize);
        ASSERTX (NO_OFFSET == rpcExpression->typeUniqueIDOffset || rpcExpression->typeUniqueIDOffset < uniqueIDsBufferSize);
        ASSERTX (NO_OFFSET == rpcExpression->symbolUniqueIDoffset || rpcExpression->symbolUniqueIDoffset < uniqueIDsBufferSize);
        auto curExpression = stapin::Expression {0}; 
        curExpression.typeFlag = static_cast<stapin::E_expression_type>(rpcExpression->type);
        curExpression.size = rpcExpression->dataSize;
        curExpression.level = rpcExpression->level;
        {
            std::lock_guard<std::mutex> lock(namesAndIDsMutex);
            auto expressionNamesBuffer = reinterpret_cast<const char*>(buffers.namesBuffer.get()->data);
            auto uniqueIDSBuffer = reinterpret_cast<const char*>(buffers.uniqueIDSBuffer.get()->data);
            curExpression.name = insert_string_to_db(expressionNamesBuffer, rpcExpression->expressionNameOffset);
            curExpression.parentName = insert_string_to_db(expressionNamesBuffer, rpcExpression->parentExpressionNameOffset);
            curExpression.typeUniqueID = insert_string_to_db(uniqueIDSBuffer, rpcExpression->typeUniqueIDOffset);
            curExpression.symbolUniqueID = insert_string_to_db(uniqueIDSBuffer, rpcExpression->symbolUniqueIDoffset);
        }
        
        if(rpcExpression->isValue && NO_OFFSET != rpcExpression->dataOffset )
        {
            ASSERTX (rpcExpression->dataOffset < dataBufferSize);
            memcpy(curExpression.data, buffers.expressionsDataBuffer.get()->data+rpcExpression->dataOffset , rpcExpression->dataSize);
        }
        offset  += sizeof(stapin::Rpc_expression) + rpcExpression->isValue * rpcExpression->dataSize;
        return curExpression;
    }

    /**
     * @brief Creating expressions ('buffers.count' number) and inserting them into the iterator.
     * @param buffers  Buffers containing buffers results from do rpc query.
     * @param resultExpressionIterator Iterator to insert expressions to.
     * @param namesBufferSize Size of the buffer containing expression names.
     * @param uniqueIDsBufferSize Size of the buffer containing unique IDs.
     */
    void process_expressions(const Buffers_for_rpc<stapin::Rpc_expression>& buffers, 
                            stapin::Expression_iterator* resultExpressionIterator,
                            uint16_t namesBufferSize,
                            uint16_t uniqueIDsBufferSize,
                            uint16_t dataBufferSize)
    {
        if(0 == buffers.count)
        {
            return;
        }
        size_t startIndex = 0;
        size_t offset = 0;
        if(resultExpressionIterator->values.end() == resultExpressionIterator->iter)
        {
            auto curExpression = process_expression(buffers, namesBufferSize, uniqueIDsBufferSize, dataBufferSize, offset);
            resultExpressionIterator->iter = resultExpressionIterator->values.insert(resultExpressionIterator->iter, curExpression);
            ++startIndex;
        }
        for (size_t i = startIndex; i < buffers.count; ++i)
        {
            auto curExpression = process_expression(buffers, namesBufferSize, uniqueIDsBufferSize, dataBufferSize, offset);
            resultExpressionIterator->values.push_back(curExpression);
        }
    }

    /**
    * @brief delete the given iterator. (work for both symbols and expressions iterator).
    * @tparam ITERATOR_TYPE  type if the iterator.
    * @param iterator iterator to delete.
    */
    template <typename ITERATOR_TYPE>
    void close_iterator(stapin::STAPIN_iterator<ITERATOR_TYPE>* iterator)
    {
        if(nullptr == iterator )
        {
            return;
        } 
        bool ret = false;
        if constexpr(std::is_same_v<ITERATOR_TYPE, stapin::Symbol>)
        {
            ret = remote::do_rpc<stapin::STAPIN_GET_SYMBOLS_END_SCHEMA, void>((uintptr_t)iterator->ID);
        }
        else if constexpr(std::is_same_v<ITERATOR_TYPE, stapin::Expression>)
        {
            ret = remote::do_rpc<stapin::STAPIN_GET_EXPRESSION_END_SCHEMA, void>((uintptr_t)iterator->ID);
        }
        if(!ret)
        {
            notify_pin_do_rpc_fail(__PRETTY_FUNCTION__);
        }
        delete iterator;
        return;
    }

    /**
    * @brief resets the given iterator. (work for both symbols and expressions iterator).
    * @tparam ITERATOR_TYPE  type if the iterator.
    * @param iterator iterator to reset.
    */
    template <typename ITERATOR_TYPE>
    void reset_iterator(stapin::STAPIN_iterator<ITERATOR_TYPE>* iterator)
    {
        if(nullptr == iterator)
        {
            return;
        }
        iterator->iter = iterator->values.begin();
        return;
    }

    /**
    * @brief checkinf if able to give next item of iterator from current position.
    * @tparam ITERATOR_TYPE the type of the iterator.
    * @tparam VALUE_TYPE the value type of the value to put the result in 
    * (same as the values inside the t_value_list in the iterator).
    * @param iterator iterator to check.
    * @param val value to put the next item of the iterator in.
    * @return bool indicating if could retrive next item or not.
    */
    template <typename VALUE_TYPE>
    bool get_next_value_from_iterator(stapin::STAPIN_iterator<VALUE_TYPE>* iterator, VALUE_TYPE* val)
    {
        if(iterator->values.end() != iterator->iter)
        {
            *val = *iterator->iter;
            ++iterator->iter;
            return true;
        }
        return false;
    }

    /**
    * @brief creating RPC Context (full), filling it and send it to the RPC.
    * @tparam SCHEMA  The RPC schema to use in the remote::do_rpc call.
    * @tparam PASS_SIZE boolean indicating if to pass the size in the remote::do_rpc call.
    * @param ctx PIN context.
    * @param pid proccess ID.
    * @param tid Thread ID.
    * @return bool indicating if all the actions were succesful.
    */
    template <t_rpc_message_schema const& SCHEMA, bool PASS_SIZE = false>
    bool do_rpc_with_context(const CONTEXT* ctx,NATIVE_TID pid, NATIVE_TID tid)
    {
        std::unique_ptr<stapin::RPC_Full_Context> rpcContext( new (std::nothrow) stapin::RPC_Full_Context);
        if(nullptr == rpcContext)
        {
            return false;
        }
        auto retVal  = false;
        auto doRpcRes = false;
        fill_context(ctx, *rpcContext);
        if constexpr(PASS_SIZE)
        {
            doRpcRes = remote::do_rpc<SCHEMA>(retVal, pid, tid, remote::rpc_buffer(rpcContext.get(),  stapin::FULL_RPC_CONTEXT_SIZE), stapin::FULL_RPC_CONTEXT_SIZE);
        }
        else
        {
            doRpcRes = remote::do_rpc<SCHEMA>(retVal, pid, tid, remote::rpc_buffer(rpcContext.get(),  stapin::FULL_RPC_CONTEXT_SIZE));
        }
        
        if (!doRpcRes || !retVal)
        {
            notify_pin_do_rpc_fail(__PRETTY_FUNCTION__);
            return false;
        }
        return true;
    }

    /**
    * @brief retriving a symbol by query (address, id, reg etc).
    * @tparam SCHEMA The RPC schema to use in the remote::do_rpc call.
    * @tparam QUERY_PARAMETER the value to pass to do_rpc call as the 4th parameter. 
    * (for example, by address  query -> special value is addres etc).
    * @param symbol symbol to put the result symbol in.
    * @param value this is the special value to send in the right place of the do rpc schema of each schema.
    * @return bool indicating if could retrive next item or not.
    */
    template <t_rpc_message_schema const& SCHEMA, typename QUERY_PARAMETER >
    bool get_symbol_by_query(stapin::Symbol* symbol, QUERY_PARAMETER value)
    {
        auto buffers = create_rpc_buffers<stapin::Rpc_symbol, true>();
        if(!buffers.success)
        {
            return false;
        }
        auto foundSymbol = false;
        auto tid = PIN_GetNativeTid();
        uint16_t namesBufferSize = Sizes<stapin::Rpc_symbol>::NAMES_BUFFER_SIZE;
        uint16_t uniqueIDsBufferSize = Sizes<stapin::Rpc_symbol>::UNIQUEIDS_BUFFER_SIZE;
        auto ret = remote::do_rpc<SCHEMA>(foundSymbol, _pid, tid, value,
                                            remote::rpc_buffer(buffers.namesBuffer.get(), namesBufferSize, RpcArgFlagsDataEmpty), 
                                            namesBufferSize,
                                            remote::rpc_buffer(buffers.uniqueIDSBuffer.get(), uniqueIDsBufferSize, RpcArgFlagsDataEmpty), 
                                            uniqueIDsBufferSize,
                                            remote::rpc_buffer(buffers.rpcValuesBuffer.get(), sizeof(stapin::Rpc_symbol), RpcArgFlagsDataEmpty));
        if(!ret || !foundSymbol)
        {
            notify_pin_do_rpc_fail(__PRETTY_FUNCTION__);
            return false;
        }
        auto resultSymbol  = process_symbol(buffers, 0, namesBufferSize, uniqueIDsBufferSize);
        *symbol = resultSymbol; 
        return true;
    }

    /**
    * @brief get symbols start logic with name or without name.
    * @param nameToSearch name of a symbol to search.
    * @param parent parent of the symbol if wanted.
    * @return stapin::Symbol_iterator* result iterator of the fecthing.
    */
    stapin::Symbol_iterator* get_symbols_do_rpc(std::string  nameToSearch, const stapin::Symbol* parent)
    {
        auto resultSymbolIterator = new (std::nothrow) stapin::Symbol_iterator{0};
        if(nullptr == resultSymbolIterator)
        {
            return nullptr;
        }
        resultSymbolIterator->iter = resultSymbolIterator->values.end();
        uintptr_t resultId = -1;
        auto tid = PIN_GetNativeTid();
        std::string parentUniqueID = parent? parent->uniqueId: "";
        auto buffers = create_rpc_buffers<stapin::Rpc_symbol>();
        if(!buffers.success)
        {
            return nullptr;
        }
        uint16_t namesBufferSize = Sizes<stapin::Rpc_symbol>::NAMES_BUFFER_SIZE;
        uint16_t uniqueIDsBufferSize = Sizes<stapin::Rpc_symbol>::UNIQUEIDS_BUFFER_SIZE;
        uint16_t rpcBufferSize = Sizes<stapin::Rpc_symbol>::RPC_BUFFER_SIZE;
        auto ret = remote::do_rpc<stapin::STAPIN_GET_SYMBOLS_START_SCHEMA>(resultId, _pid,  tid,  
                                                                            parentUniqueID, 
                                                                            nameToSearch,
                                                                            remote::rpc_buffer(buffers.namesBuffer.get(), namesBufferSize, RpcArgFlagsDataEmpty),
                                                                            namesBufferSize,
                                                                            remote::rpc_buffer(buffers.uniqueIDSBuffer.get(), uniqueIDsBufferSize, RpcArgFlagsDataEmpty), 
                                                                            uniqueIDsBufferSize,
                                                                            remote::rpc_buffer(buffers.rpcValuesBuffer.get(), rpcBufferSize, RpcArgFlagsDataEmpty), 
                                                                            buffers.count);
        if(!ret || -1 == resultId)
        {
            notify_pin_do_rpc_fail(__PRETTY_FUNCTION__);
            return nullptr;
        }
        process_symbols(buffers, resultSymbolIterator, namesBufferSize,uniqueIDsBufferSize );
        resultSymbolIterator->ID  = resultId;
        return resultSymbolIterator;
    }

}//anonymus namespace 

bool stapin::init(){
    auto returnVal = true;
    auto pid = getpid();
    _pid = pid;
    auto ret = remote::do_rpc< stapin::STAPIN_INIT_SCHEMA >(returnVal, pid);
    if (!ret || !returnVal)
    {
        notify_pin_do_rpc_fail(__PRETTY_FUNCTION__);
        return false;
    }
    bufferPool = bufferpool::Buffer_pool::create(BUFFER_POOL_INIT_CAPACITY);
    return nullptr != bufferPool;
}

bool stapin::notify_image_load(IMG img) 
{
    // Calculate the total size needed for section names and Rpc_sections
    size_t sectionNamesSize = 0;
    uint16_t numSections = 0;
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec), ++numSections) 
    {
        sectionNamesSize += SEC_Name(sec).length() + 1; // +1 for null terminator
    }

    // Allocate buffers for section names and Rpc_sections
    std::unique_ptr<char[]> sectionNamesBuffer( new (std::nothrow) char[sectionNamesSize]);
    std::unique_ptr<stapin::Rpc_section[]> RpcSectionsBuffer(new (std::nothrow) stapin::Rpc_section[numSections]);
    if(nullptr == sectionNamesBuffer || nullptr == RpcSectionsBuffer )
    {
        return false;
    }
    // Fill the buffers
    auto namePtr = sectionNamesBuffer.get();
    memset(namePtr, 0, sectionNamesSize);
    size_t sectionIndex = 0;
    uint16_t nameOffset = 0;
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) 
    {
        // Copy section name
        const auto& curName = SEC_Name(sec);
        memcpy(namePtr, curName.data(), curName.length());
        namePtr += curName.length() + 1;
        // Fill Rpc_section
        auto& rpcSection = RpcSectionsBuffer[sectionIndex++];
        rpcSection.sectionNameOffset = nameOffset;
        rpcSection.sectionAddress = SEC_Address(sec);
        rpcSection.sectionSize = SEC_Size(sec);
        rpcSection.fileOffset = SEC_Address(sec) - IMG_LowAddress(img);
        int8_t flags = 0;
        flags |= SEC_IsReadable(sec) ? FLAG_R : 0;   // Set MM_FLAG_R if sec is readable
        flags |= SEC_IsWriteable(sec) ? FLAG_W : 0;  // Set MM_FLAG_W if sec is writable
        flags |= SEC_IsExecutable(sec) ? FLAG_X : 0; // Set MM_FLAG_X if sec is executable
  
        rpcSection.mmFlags = flags;

        int8_t bss = SEC_TYPE_BSS == SEC_Type(sec)? 1: 0; 

        rpcSection.isBSS = bss;
        nameOffset += curName.length() + 1;
    }
    auto retValue = true;
    auto&& imgName = IMG_Name(img);
    auto ret = remote::do_rpc< stapin::STAPIN_IMAGE_LOAD_SCHEMA >(
        retValue, _pid,
        imgName,
        remote::rpc_buffer(sectionNamesBuffer.get(), sectionNamesSize),
        remote::rpc_buffer(RpcSectionsBuffer.get(), numSections * sizeof(stapin::Rpc_section)),
        numSections
    );
    if (!ret || !retValue) 
    {
        notify_pin_do_rpc_fail(__PRETTY_FUNCTION__);
        return false;
    }
    return true;
}

bool stapin::notify_image_unload(IMG img){
    auto retValue = true;
    auto&& imgName = IMG_Name(img);
    auto ret = remote::do_rpc<stapin::STAPIN_IMAGE_UNLOAD_SCHEMA>(retValue, _pid,  imgName);
    if (!ret || !retValue)
    {
        notify_pin_do_rpc_fail(__PRETTY_FUNCTION__);
        return false;
    }
    return true;
}

bool stapin::notify_thread_start(const CONTEXT* ctx, THREADID threadId){
    if(nullptr == ctx)
    {
        return false;
    }
    auto tid = PIN_GetNativeTidFromThreadId(threadId);
    return do_rpc_with_context<stapin::STAPIN_THREAD_START_SCHEMA>(ctx, _pid, tid);
}

bool stapin::notify_thread_fini(THREADID threadId){
    auto tid = PIN_GetNativeTidFromThreadId(threadId);
    auto retVal = true;
    auto ret = remote::do_rpc< stapin::STAPIN_THREAD_FINI_SCHEMA >(retVal,  _pid, tid);
    if (!ret || !retVal)
    {
        notify_pin_do_rpc_fail(__PRETTY_FUNCTION__);
        return false;
    }
    return true;

}

bool stapin::notify_fork_in_child(const CONTEXT* ctx, THREADID threadId){
    if(nullptr == ctx ){
        return false;
    }
    auto newPID = getpid();
    auto res = do_rpc_with_context<stapin::STAPIN_FORK_IN_CHILD_SCHEMA>(ctx, newPID, _pid);
    _pid = newPID;
    return res;
}

bool stapin::set_context(const CONTEXT* ctx, stapin::E_context_update_method contextUpdateMethod){
    if(nullptr == ctx )
    {
        return false;
    }
    auto retVal = true;
    auto tid = PIN_GetNativeTid();
    bool ret = true;
    switch (contextUpdateMethod)
    {
        case E_context_update_method::FULL:
            {
                ret = do_rpc_with_context<stapin::STAPIN_SET_CONTEXT_SCHEMA, true>(ctx, _pid, tid);
                break;
            }
        case E_context_update_method::FAST:
            {
                std::unique_ptr<stapin::RPC_Full_Context> rpcContext( new (std::nothrow) stapin::RPC_Full_Context);
                if(nullptr == rpcContext)
                {
                    ret = false;
                }
                else
                {
                    fill_partial_context(ctx, rpcContext.get()->partial_context);
                    ret = remote::do_rpc< stapin::STAPIN_SET_CONTEXT_SCHEMA >(retVal, _pid , tid , 
                                                                        remote::rpc_buffer(rpcContext.get(), stapin::PARTIAL_RPC_CONTEXT_SIZE), 
                                                                        stapin::PARTIAL_RPC_CONTEXT_SIZE);
                }
                break;
            }
        case E_context_update_method::IP_ONLY:
            {
                auto ip = get_IP_from_pin_context(ctx);
                ret = remote::do_rpc< stapin::STAPIN_SET_IP_SCHEMA >(retVal, _pid , tid , ip);
                break; 
            }
        }
    if (!ret || !retVal)
    {
        notify_pin_do_rpc_fail(__PRETTY_FUNCTION__);
        return false;
    }
    return true;

}

size_t stapin::get_source_locations(ADDRINT startAddress, ADDRINT endAddress, stapin::Source_loc* locations, size_t locCount){
    if(nullptr == locations ||  0 == locCount|| startAddress > endAddress)
    {
        return 0;
    }
    size_t result = 0;
    auto bufferSize = SOURCE_FILE_NAMES_BUFFER_SIZE;
    std::unique_ptr<char[]> sourceFileNames(new (std::nothrow) char[bufferSize]);
    std::unique_ptr<stapin::Rpc_source_location[]> rpcSourceLocations(new (std::nothrow) stapin::Rpc_source_location[locCount]);
    if(nullptr == sourceFileNames || nullptr == rpcSourceLocations)
    {
        return 0;
    }
    auto ret = remote::do_rpc<stapin::STAPIN_GET_SOURCE_LOCATIONS_SCHEMA>(result, _pid,  startAddress, endAddress, 
                                                                        remote::rpc_buffer(sourceFileNames.get(), bufferSize, RpcArgFlagsDataEmpty),
                                                                        bufferSize,  
                                                                        remote::rpc_buffer(rpcSourceLocations.get(), locCount*sizeof(stapin::Rpc_source_location), RpcArgFlagsDataEmpty),
                                                                        locCount);
    if(!ret){
        notify_pin_do_rpc_fail(__PRETTY_FUNCTION__);
        return 0;
    }
    for (size_t i = 0; i < result; ++i)
    {
        const auto& curRPCLoc = rpcSourceLocations[i];
        locations[i].startLine = curRPCLoc.startLine;
        locations[i].endLine = curRPCLoc.endLine;
        locations[i].startAddress = curRPCLoc.startAddress;
        locations[i].endAddress = curRPCLoc.endAddress;
        locations[i].startColumn = curRPCLoc.startColumn;
        locations[i].endColumn = curRPCLoc.endColumn;
        locations[i].nextAddress = curRPCLoc.nextAddress;
        locations[i].nextStmtAddress = curRPCLoc.nextStmtAddress;

        std::lock_guard<std::mutex> lock(namesAndIDsMutex);
        auto insNameResult = namesAndIDs.insert(&sourceFileNames[curRPCLoc.sourceFileOffset]);
        locations[i].srcFileName = (*insNameResult.first).c_str();
    }
    return result;
}

stapin::Symbol_iterator* stapin::get_symbols(const stapin::Symbol* parent){
    return get_symbols_do_rpc("", parent);
}

bool stapin::get_next_symbol(stapin::Symbol_iterator* iterator, stapin::Symbol* symbol){
    if (nullptr == iterator )
    {
        return false;
    }
    if(get_next_value_from_iterator(iterator, symbol))
    {
        return true;
    }
    auto gotSymbols = false;
    auto buffers = create_rpc_buffers<stapin::Rpc_symbol>();
    if(!buffers.success)
    {
        return false;
    }
    uint16_t namesBufferSize = Sizes<stapin::Rpc_symbol>::NAMES_BUFFER_SIZE;
    uint16_t uniqueIDsBufferSize = Sizes<stapin::Rpc_symbol>::UNIQUEIDS_BUFFER_SIZE;
    uint16_t rpcBufferSize = Sizes<stapin::Rpc_symbol>::RPC_BUFFER_SIZE;
    auto ret = remote::do_rpc<stapin::STAPIN_GET_SYMBOLS_NEXT_SCHEMA>(gotSymbols, (uintptr_t)iterator->ID,  
                                                                        remote::rpc_buffer(buffers.namesBuffer.get(), namesBufferSize, RpcArgFlagsDataEmpty), 
                                                                        namesBufferSize,
                                                                        remote::rpc_buffer(buffers.uniqueIDSBuffer.get(), uniqueIDsBufferSize ,RpcArgFlagsDataEmpty), 
                                                                        uniqueIDsBufferSize,
                                                                        remote::rpc_buffer(buffers.rpcValuesBuffer.get(), rpcBufferSize, RpcArgFlagsDataEmpty), 
                                                                        buffers.count);
    
    if(!ret || !gotSymbols)
    {
        if(!ret)
        {
            notify_pin_do_rpc_fail(__PRETTY_FUNCTION__);
        }
        return false;
    }
    process_symbols(buffers, iterator, namesBufferSize,uniqueIDsBufferSize );
    *symbol = *iterator->iter;   
    ++iterator->iter;
    return true;
}

void stapin::reset_symbols_iterator(Symbol_iterator* iterator){
    reset_iterator(iterator);
}

stapin::Symbol_iterator* stapin::find_symbol_by_name(const char* symbolName, const stapin::Symbol* parent){
    if(nullptr == symbolName || NULL_TERMINATOR == symbolName[0] )
    {
        return nullptr;
    }
    return get_symbols_do_rpc(symbolName, parent);
}

void stapin::close_symbol_iterator(stapin::Symbol_iterator* iterator){
    close_iterator(iterator);
}

bool stapin::get_symbol_by_address(ADDRINT address, stapin::Symbol* symbol){
    if(nullptr == symbol)
    {
        return false;
    }
    return get_symbol_by_query<stapin::STAPIN_GET_SYMBOL_BY_ADDR_SCHEMA>(symbol, address);
}

bool stapin::get_symbol_by_reg(REG reg, stapin::Symbol* symbol){
    if( nullptr == symbol)
    {
        return false;
    }
    auto regIndex = find_index_of_reg(reg);
    if(0 == regIndex)
    {
        return false;
    }
    return get_symbol_by_query<stapin::STAPIN_GET_SYMBOL_BY_REG_SCHEMA>(symbol, (unsigned)regIndex);
}

bool stapin::get_symbol_by_id(const char* uniqueId, stapin::Symbol* symbol){
    if(nullptr == uniqueId || nullptr == symbol || NULL_TERMINATOR == uniqueId[0])
    {
        return false;
    }
    return get_symbol_by_query<stapin::STAPIN_GET_SYMBOL_BY_ID_SCHEMA>(symbol, remote::rpc_buffer(uniqueId, SINGLE_ID_SIZE));
}

bool stapin::get_rtn_symbol(RTN rtn, stapin::Symbol* symbol){
    if( nullptr == symbol)
    {
        return false;
    }
    auto address = RTN_Address(rtn);
    return stapin::get_symbol_by_address(address, symbol);
}

stapin::Expression_iterator* stapin::evaluate_expression(const std::string& expression)
{
    if(expression.empty())
    {
        return nullptr;
    }
    auto resultExpressionIterator = new (std::nothrow) stapin::Expression_iterator{0};
    if(nullptr == resultExpressionIterator)
    {
        return nullptr;
    }
    resultExpressionIterator->iter = resultExpressionIterator->values.end();
    uintptr_t resultId = -1;
    auto tid = PIN_GetNativeTid();
    auto buffers = create_rpc_buffers<stapin::Rpc_expression>();
    if(!buffers.success)
    {
        return nullptr;
    }
    uint16_t namesBufferSize = Sizes<stapin::Rpc_expression>::NAMES_BUFFER_SIZE;
    uint16_t uniqueIDsBufferSize = Sizes<stapin::Rpc_expression>::UNIQUEIDS_BUFFER_SIZE;
    uint16_t rpcBufferSize = Sizes<stapin::Rpc_expression>::RPC_BUFFER_SIZE;
    uint16_t dataBufferSize = Sizes<stapin::Rpc_expression>::DATA_BUFFER_SIZE;
    auto ret = remote::do_rpc<stapin::STAPIN_GET_EXPRESSION_START_SCHEMA>(resultId, _pid,  tid,
                                                                        expression,
                                                                        remote::rpc_buffer(buffers.namesBuffer.get(), namesBufferSize, RpcArgFlagsDataEmpty), 
                                                                        namesBufferSize,
                                                                        remote::rpc_buffer(buffers.uniqueIDSBuffer.get(),uniqueIDsBufferSize,RpcArgFlagsDataEmpty), 
                                                                        uniqueIDsBufferSize,
                                                                        remote::rpc_buffer(buffers.rpcValuesBuffer.get(), rpcBufferSize ,RpcArgFlagsDataEmpty), 
                                                                        buffers.count,
                                                                        remote::rpc_buffer(buffers.expressionsDataBuffer.get(), dataBufferSize ,RpcArgFlagsDataEmpty),
                                                                        dataBufferSize);
    if(!ret || -1 == resultId)
    {
        notify_pin_do_rpc_fail(__PRETTY_FUNCTION__);
        return nullptr;
    }
    process_expressions(buffers, resultExpressionIterator, namesBufferSize, uniqueIDsBufferSize, dataBufferSize);
    resultExpressionIterator->ID  = resultId;
    return resultExpressionIterator;
}

void stapin::reset_expressions_iterator(stapin::Expression_iterator* iterator)
{
    reset_iterator(iterator);
}

bool stapin::get_next_expression(stapin::Expression_iterator* iterator, stapin::Expression* expression)
{
    if (nullptr == iterator )
    {
        return false;
    }
    if(get_next_value_from_iterator(iterator, expression))
    {
        return true;
    }
    auto buffers = create_rpc_buffers<stapin::Rpc_expression>();
    if(!buffers.success)
    {
        return false;
    }
    auto gotExpressions = false;
    uint16_t namesBufferSize = Sizes<stapin::Rpc_expression>::NAMES_BUFFER_SIZE;
    uint16_t uniqueIDsBufferSize = Sizes<stapin::Rpc_expression>::UNIQUEIDS_BUFFER_SIZE;
    uint16_t dataBufferSize = Sizes<stapin::Rpc_expression>::DATA_BUFFER_SIZE;
    auto ret = remote::do_rpc<stapin::STAPIN_GET_EXPRESSION_NEXT_SCHEMA>(gotExpressions, (uintptr_t)iterator->ID,
                                                                        remote::rpc_buffer(buffers.namesBuffer.get(), namesBufferSize, RpcArgFlagsDataEmpty), 
                                                                        namesBufferSize,
                                                                        remote::rpc_buffer(buffers.uniqueIDSBuffer.get(),uniqueIDsBufferSize,RpcArgFlagsDataEmpty), 
                                                                        uniqueIDsBufferSize,
                                                                        remote::rpc_buffer(buffers.rpcValuesBuffer.get(), Sizes<stapin::Rpc_expression>::RPC_BUFFER_SIZE, RpcArgFlagsDataEmpty), 
                                                                        buffers.count,
                                                                        remote::rpc_buffer(buffers.expressionsDataBuffer.get(), dataBufferSize ,RpcArgFlagsDataEmpty),
                                                                        dataBufferSize);
    if(!ret)
    {
        notify_pin_do_rpc_fail(__PRETTY_FUNCTION__);
        return false;
    }
    if(!gotExpressions)
    {
        return false;
    }
    process_expressions(buffers, iterator, namesBufferSize, uniqueIDsBufferSize, dataBufferSize);
    *expression = *iterator->iter;
    ++iterator->iter;
    return true;
}
void stapin::close_expression_iterator(stapin::Expression_iterator* iterator)
{
    close_iterator(iterator);
}

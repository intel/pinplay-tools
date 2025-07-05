/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#include <set>

#include <rscproto.h>
#include <ipind_plugin.h>
#include "stapin-plugin-TCF-and-context-logic.h"
#include "stapin-plugin-image-logic.h"
#include "stapin-plugin-expressions-logic.h"
#include "stapin-plugin-symbols-logic.h"
#include "stapin-plugin-sourceLocation-logic.h"
#include "server-utils.h"

namespace{
    const std::string ALLOCATION_FAIL = "An allocation failure occurred";
    const std::string NULL_POINTER_FAIL = "A reference to null pointer fail";
    const std::string INSERTION_FAIL = "Failed to insert to data base";
    const std::string SECTIONS_NAMES_SIZE_ZERO = "Got 0 for the sections names buffer size";
    const std::string ADD_THREAD_FAIL = "Could not add thread to the tcf context";
    const std::string UPDATE_RPC_CONTEXT_FAIL = "Could not update rpc context in the tcf context";
    const std::string IMAGE_NAME_SIZE_ZERO = "Got 0 in the size of image name";
    const std::string BUFFER_SIZE_FAIL = "A buffer size problem occurred";
    const std::string REG_INDEX_FAIL = "Register index is out of bounds";
    constexpr size_t MAX_EXPRESSION_DATA_SIZE = 64;

    enum QUERY{ADDRESS, REG, ID};
    struct Symbol_query_params {
        stapinlogic::Symbol_by_query& iterator;
        uint16_t wantedNameSize;
        uint16_t wantedUniqueIDSize;
        t_rpc_arg* rpcArgs;
        t_rpc_ret* retRpcArg;
        const t_rpc_message_schema schema;
        std::string failureReason;
    };

    /**
     * @brief Buffer sizes structure.
     * This structure holds various buffer sizes and a counter for processed symbols or expressions.
     */
    struct Buffer_sizes
    {
        size_t itemsBufferSize;       //!< Size of the items buffer
        size_t itemNamesbufferSize;   //!< Size of the item names buffer
        size_t uinqueIDSbufferSize;   //!< Size of the unique IDs buffer
        size_t dataBufferSize;        //!< Size of the data buffer (for expressions!).
        uint16_t counter;             //!< Counter for processed symbols or expressions.
    };

    /*  HELPER FUNCTIONS */

    /**
    * @brief Marks rpc args as On parameters flag.
    * @param indexes the indexes of the rpc args.
    * @param rpcArgs A pointer to t_rpc_arg. 
    */
    void mark_in_paramter_rpc_args(const std::set<int>& indexes, t_rpc_arg* rpcArgs){
        for (auto index : indexes) 
        {
            rpcArgs[index].flags = RpcArgFlagsDataEmpty;
        }  
    }

    /**
    * @brief fills the return rpc arg with the wanted value.
    * @tparam RET_TYPE type of return value for schema.
    * @param retRpcArg A pointer t_rpc_ret, the rpc return arg we want to assign to.
    * @param returnSchema The return schema to assign. 
    * @param res The result value to assign. 
    * @param sizeOfRes size of the result value. 
    */
    template<typename RET_TYPE>
    void fill_return_rpc_arg(t_rpc_ret* retRpcArg,  t_rpc_arg_schema returnSchema, RET_TYPE res){
        retRpcArg->argSchema = returnSchema;
        retRpcArg->argData = (uint64_t)res;
        retRpcArg->argDataSize = sizeof(RET_TYPE);
        retRpcArg->flags = RpcArgFlagsNone;
    }

    /**
    * @brief fill the rpc arg buffer.
    * @param rpcArg the rpc arg buffer.
    * @param argData the result value to assign.
    * @param argSchema the result arg schema to assign.
    * @param BufferSize the buffer size to assign.
    */
    void fill_rpc_buffer(t_rpc_arg& rpcArg, uint64_t argData, t_rpc_arg_schema argSchema, size_t BufferSize){
        if (rpcArg.deleter)
        {
            // Free data currently in arg if argData was allocated for us by pind
            rpcArg.deleter(reinterpret_cast< void* >(rpcArg.argData));
        }
        rpcArg.deleter = free;
        rpcArg.argData = argData;
        rpcArg.argSchema = argSchema;
        rpcArg.argDataSize = BufferSize;
        rpcArg.flags = BufferSize == 0? RpcArgFlagsDataEmpty :RpcArgFlagsNone;
    }

    /**
     * @brief Handles the processing of a symbol query by filling the necessary result buffers based on the query's outcome.
     * This function checks if the symbol was found and whether the buffer sizes are sufficient. It then fills the buffers
     * with the symbol's name, unique ID, and other relevant data. If the symbol is not found or if there is insufficient
     * buffer space, it sets the appropriate return value.
     * @param params A reference to a Symbol_query_params struct containing all necessary parameters for handling the symbol query.
     * @return boolean indicating if the operation was a success or not.
     */
    bool handle_symbol_query(Symbol_query_params& params) 
    {
        auto curMaxUniqueIDBufferSize = params.wantedUniqueIDSize < stapinlogic::MAX_UINQUE_IDS_BUFFER_SIZE ? params.wantedUniqueIDSize : stapinlogic::MAX_UINQUE_IDS_BUFFER_SIZE;
        auto curMaxNameBufferSize = params.wantedNameSize < stapinlogic::MAX_SYMBOLS_NAMES_BUFFER_SIZE ? params.wantedNameSize : stapinlogic::MAX_SYMBOLS_NAMES_BUFFER_SIZE;

        auto found = params.iterator.found;
        if (!found || params.iterator.symbolName.length() + 1 >= curMaxNameBufferSize ||
            params.iterator.uniqueID.length() + 1 + params.iterator.typeUniqueID.length() + 1 >= curMaxUniqueIDBufferSize) 
        {
            params.failureReason = BUFFER_SIZE_FAIL;
            return false;
        }

        uint16_t symbolNameBufferSize = params.iterator.symbolName.length() + 1;
        uint16_t uniqueIDBufferSize = params.iterator.uniqueID.length() + 1 + params.iterator.typeUniqueID.length() + 1 + params.iterator.baseTypeUniqueID.length() + 1;

        stapinlogic::t_char_array sourceSymbolsNames {(char*)malloc(symbolNameBufferSize)};
        if(nullptr == sourceSymbolsNames)
        {
            params.failureReason = ALLOCATION_FAIL;
            return false;
        }
        auto resultSymbol = (stapin::Rpc_symbol*)params.iterator.resultSymbol.get();
        if(params.iterator.symbolName.empty())
        {
           resultSymbol->nameOffset = stapinlogic::NO_OFFSET;
        }
        else
        {
            strcpy(sourceSymbolsNames.get(), params.iterator.symbolName.c_str());
            resultSymbol->nameOffset = 0;
        }

        stapinlogic::t_char_array UinqueIDS {(char*)malloc(uniqueIDBufferSize)};
        if(nullptr == UinqueIDS)
        {
            params.failureReason = ALLOCATION_FAIL;
            return false;
        }
        if (nullptr != UinqueIDS.get()) 
        {
            size_t requiredUniqueIDSize = params.iterator.uniqueID.length();
            size_t requiredTypeUniqueIDSize = params.iterator.typeUniqueID.length();
            size_t requiredBaseTypeUniqueIDSize = params.iterator.baseTypeUniqueID.length();
            memcpy(UinqueIDS.get(), params.iterator.uniqueID.c_str(), requiredUniqueIDSize);
            UinqueIDS.get()[requiredUniqueIDSize] = stapinlogic::NULL_TERMINATOR;
            memcpy(UinqueIDS.get() + requiredUniqueIDSize + 1, params.iterator.typeUniqueID.c_str(), requiredTypeUniqueIDSize);
            UinqueIDS.get()[requiredUniqueIDSize + 1 + requiredTypeUniqueIDSize] = stapinlogic::NULL_TERMINATOR;
            memcpy(UinqueIDS.get() + requiredUniqueIDSize + requiredTypeUniqueIDSize + 2, params.iterator.baseTypeUniqueID.c_str(), requiredBaseTypeUniqueIDSize);
            UinqueIDS.get()[requiredUniqueIDSize + 2 + requiredTypeUniqueIDSize + requiredBaseTypeUniqueIDSize] = stapinlogic::NULL_TERMINATOR;
        }

        resultSymbol->uniqueIdOffset = !params.iterator.uniqueID.empty()? 0: stapinlogic::NO_OFFSET;
        resultSymbol->typeIdOffset = !params.iterator.typeUniqueID.empty()? (uint16_t)params.iterator.uniqueID.length() + 1 :stapinlogic::NO_OFFSET;
        resultSymbol->baseTypeIdOffset = !params.iterator.baseTypeUniqueID.empty()?  (uint16_t)resultSymbol->typeIdOffset + params.iterator.typeUniqueID.length() + 1 : stapinlogic::NO_OFFSET;

        fill_rpc_buffer(params.rpcArgs[3], (uint64_t)sourceSymbolsNames.release(), params.schema.argSchemaArray[3], symbolNameBufferSize);
        fill_rpc_buffer(params.rpcArgs[5], (uint64_t)UinqueIDS.release(), params.schema.argSchemaArray[5], uniqueIDBufferSize);
        fill_rpc_buffer(params.rpcArgs[7], (uint64_t)params.iterator.resultSymbol.release(), params.schema.argSchemaArray[7], sizeof(stapin::Rpc_symbol));

        params.rpcArgs[4].argData = (uint64_t)symbolNameBufferSize;
        params.rpcArgs[6].argData = (uint64_t)uniqueIDBufferSize;

        fill_return_rpc_arg(params.retRpcArg, params.schema.returnValueSchema, found);
        return true;
    }
 
    /**
     * @brief Inserts symbol data into the registry.
     * This function processes a queue of symbol attributes, updating buffer sizes and counters as it goes.
     * It ensures that names and unique IDs are only counted once.
     * @param tempQueue A queue of symbol attributes to process.
     * @param symbolsCount The total number of symbols to process.
     * @param symbolBufferSize Reference to the size of the symbol buffer.
     * @param symbolNamesbufferSize Reference to the size of the symbol names buffer.
     * @param uniqueIDSbufferSize Reference to the size of the unique IDs buffer.
     * @param counter Reference to the counter for processed symbols.
     * @return true on success, false otherwise.
     */
    bool insert_symbol_data(const std::deque<stapinlogic::Symbol_attributes>& symAttrsQueue, 
        const uint16_t symbolsCount, 
        Buffer_sizes& bufferSizes)
    {
        std::set<std::string> foundNamesSet;
        auto it = symAttrsQueue.begin();
        while(it != symAttrsQueue.end() && bufferSizes.counter < symbolsCount)
        {
            const auto& symbol = *it;

            // Use references to avoid unnecessary string copies
            const std::string& name = symbol.name;
            const std::string& uniqueID = symbol.uniqueID;
            const std::string& typeUniqueID = symbol.typeUniqueID;
            const std::string& baseTypeUniqueID = symbol.baseTypeUniqueID;

            // Check and insert symbol.name
            if(foundNamesSet.find(name) == foundNamesSet.end())
            {
                if(!foundNamesSet.insert(name).second)
                {
                    return false;
                }
                bufferSizes.itemNamesbufferSize += name.length() + 1;
            }

            // Check and insert symbol.uniqueID
            if(foundNamesSet.find(uniqueID) == foundNamesSet.end())
            {
                if(!foundNamesSet.insert(uniqueID).second)
                {
                    return false;
                }
                bufferSizes.uinqueIDSbufferSize += uniqueID.length() + 1;
            }

            // Check and insert symbol.typeUniqueID
            if(foundNamesSet.find(typeUniqueID) == foundNamesSet.end())
            {
                if(!foundNamesSet.insert(typeUniqueID).second)
                {
                    return false;
                }
                bufferSizes.uinqueIDSbufferSize += typeUniqueID.length() + 1;
            }

            // Check and insert symbol.baseTypeUniqueID
            if(foundNamesSet.find(baseTypeUniqueID) == foundNamesSet.end())
            {
                if(!foundNamesSet.insert(baseTypeUniqueID).second)
                {
                    return false;
                }
                bufferSizes.uinqueIDSbufferSize += baseTypeUniqueID.length() + 1;
            }

            bufferSizes.itemsBufferSize += sizeof(stapin::Rpc_symbol); 
            ++bufferSizes.counter;
            ++it;
        }
        return true;
    }

    /**
     * @brief Inserts expression data into the registry.
     * This function processes a queue of expression attributes, updating buffer sizes and counters as it goes.
     * It ensures that names and unique IDs are only counted once.
     * @param tempQueue A queue of expression attributes to process.
     * @param expressionsCount The total number of expressions to process.
     * @param expressionBufferSize Reference to the size of the expression buffer.
     * @param expressionsNamesbufferSize Reference to the size of the expression names buffer.
     * @param uniqueIDSbufferSize Reference to the size of the unique IDs buffer.
     * @param counter Reference to the counter for processed expressions.
     * @return true on success, false otherwise.
     */
    bool insert_expression_data(const std::deque<stapinlogic::Expression_attributes>& exprAttrsQueue, 
        const uint16_t expressionsCount, 
        Buffer_sizes& bufferSizes)
    {
        std::set<std::string> foundNamesSet;
        auto it = exprAttrsQueue.begin();
        while(it != exprAttrsQueue.end() && bufferSizes.counter < expressionsCount)
        {
            const auto& expr = *it;

            // Use references to avoid unnecessary string copies
            const std::string& name = expr.name;
            const std::string& parentName = expr.parentName;
            const std::string& typeUniqueID = expr.typeUniqueID;
            const std::string& symbolUniqueID = expr.symbolUniqueID;

            // Check and insert expr.name
            if(foundNamesSet.find(name) == foundNamesSet.end())
            {
                if(!foundNamesSet.insert(name).second)
                {
                    return false;
                }
                bufferSizes.itemNamesbufferSize += name.length() + 1;
            }

            // Check and insert expr.parentName
            if(foundNamesSet.find(parentName) == foundNamesSet.end())
            {
                if(!foundNamesSet.insert(parentName).second)
                {
                    return false;
                }
                bufferSizes.itemNamesbufferSize += parentName.length() + 1;
            }

            // Check and insert expr.typeUniqueID
            if(foundNamesSet.find(typeUniqueID) == foundNamesSet.end())
            {
                if(!foundNamesSet.insert(typeUniqueID).second)
                {
                    return false;
                }
                bufferSizes.uinqueIDSbufferSize += typeUniqueID.length() + 1;
            }

            // Check and insert expr.symbolUniqueID
            if(foundNamesSet.find(symbolUniqueID) == foundNamesSet.end())
            {
                if(!foundNamesSet.insert(symbolUniqueID).second)
                {
                    return false;
                }
                bufferSizes.uinqueIDSbufferSize += symbolUniqueID.length() + 1;
            }

            // Check and update data buffer size
            if(!expr.data.empty())
            {
                bufferSizes.dataBufferSize += expr.dataSize;
            }

            bufferSizes.itemsBufferSize += sizeof(stapin::Rpc_expression) + expr.data.size(); 
            ++bufferSizes.counter;
            ++it;
            }
        return true;
    }

    /**
     * @brief Inserts an empty string into the registry.
     * This function attempts to insert an empty string into both the name2index and uniqueID2index maps of the registry.
     * It ensures thread safety by using a mutex lock.
     * @param registry A pointer to the registry where the empty string will be inserted.
     * @return true if both insertions are successful, false otherwise.
     */
    bool insert_empty_string_to_db(stapinlogic::Registry* registry)
    {
        auto emptyNameInsert = false;
        auto emptyIDInsert = false;
        {
            //for currupted symbols or expressions (names/IDs)!
            std::lock_guard<std::mutex> lock(registry->mtx);
            emptyNameInsert = registry->name2index.insert(std::make_pair("", stapinlogic::NO_OFFSET)).second;
            emptyIDInsert = registry->uniqueID2index.insert(std::make_pair("", stapinlogic::NO_OFFSET)).second;
        }
        if(!emptyNameInsert || !emptyIDInsert)
        {
            registry->failureReason = INSERTION_FAIL;
            return false;
        }
        return true;
    }

    /**
     * @brief Fills common attributes of the registry.
     * This function initializes various attributes of the registry, including buffer sizes and pointers to allocated memory.
     * It ensures that the buffer sizes are set to the minimum of the provided sizes and allocates memory for the buffers.
     * @param registry A pointer to the registry to be filled.
     * @param sourceBufferSizes Reference to the source buffer sizes.
     * @param bufferSizes Reference to the buffer sizes to be used.
     * @return true if all allocations are successful, false otherwise.
     */
    bool fill_common_attributes_registry(stapinlogic::Registry* registry, 
                        Buffer_sizes& sourceBufferSizes,
                        Buffer_sizes& bufferSizes)
    {
        registry->parentUinqueID = "";
        sourceBufferSizes.itemNamesbufferSize = bufferSizes.itemNamesbufferSize < sourceBufferSizes.itemNamesbufferSize? bufferSizes.itemNamesbufferSize: sourceBufferSizes.itemNamesbufferSize;
        sourceBufferSizes.uinqueIDSbufferSize = bufferSizes.uinqueIDSbufferSize < sourceBufferSizes.uinqueIDSbufferSize? bufferSizes.uinqueIDSbufferSize: sourceBufferSizes.uinqueIDSbufferSize;
        sourceBufferSizes.itemsBufferSize = bufferSizes.itemsBufferSize < sourceBufferSizes.itemsBufferSize? bufferSizes.itemsBufferSize: sourceBufferSizes.itemsBufferSize;
        registry->names  = stapinlogic::t_char_array{(char*)malloc(sourceBufferSizes.itemNamesbufferSize)};
        registry->uniqueIDS  = stapinlogic::t_char_array{(char*)malloc(sourceBufferSizes.uinqueIDSbufferSize)};
        registry->rpcItemsArray  = stapinlogic::t_rpc_array{(uint8_t*)malloc(sourceBufferSizes.itemsBufferSize)};
        if(nullptr == registry->names || nullptr == registry->uniqueIDS || nullptr == registry->rpcItemsArray)
        {
            registry->failureReason = ALLOCATION_FAIL;
            return false;
        }
        registry->rpcItemsArraySize = sourceBufferSizes.itemsBufferSize;
        registry->itemsCount = bufferSizes.counter;
        registry->curNamesFillingIndex = 0;
        registry->curUniqueIDFillingIndex = 0;
        registry->namesBufferCapacity = sourceBufferSizes.itemNamesbufferSize;
        registry->uniqueIDsBufferCapacity = sourceBufferSizes.uinqueIDSbufferSize;
        return true;
    }

    /**
     * @brief Initializes a registry with specified buffer sizes and item count.
     * This function allocates memory for storing names, unique IDs, and RPC data. It sets up the initial
     * state of the registry, preparing it for use in storing and managing items.
     * @tparam ITERATOR The type of the iterator used to traverse the items.
     * @param registry Pointer to the Registry structure to be initialized.
     * @param sourceNamesbufferSize The size of the buffer for storing names.
     * @param sourceUniqueIDSbufferSize The size of the buffer for storing unique IDs.
     * @param sourceRpcbufferSize The size of the buffer for storing RPC data.
     * @param itemsCount The initial count of items to be managed in the registry.
     * @param iterator The iterator used to traverse the items.
     * @return true on success, false otherwise.
     */
    template<typename ITERATOR>
    bool init_registry(stapinlogic::Registry* registry, 
                        Buffer_sizes& sourceBufferSizes,
                        const uint16_t itemsCount, 
                        const ITERATOR* iterator)
    {
        if(nullptr == iterator || nullptr == registry)
        {
            registry->failureReason = NULL_POINTER_FAIL;
            return false;
        }
        if(!insert_empty_string_to_db(registry))
        {
            return false;
        }
        registry->curIndex = 0;
        registry->curDataIndex = 0;
        Buffer_sizes bufferSizes {0};
        if constexpr(std::is_same_v<ITERATOR, stapinlogic::Symbols_iterator>)
        {
            auto tempQueue = iterator->symbols;
            if(!insert_symbol_data(tempQueue, itemsCount, bufferSizes))
            {
                registry->failureReason = INSERTION_FAIL;
                return false;
            }
        }
        else if constexpr(std::is_same_v<ITERATOR, stapinlogic::Expression_iterator>)
        {
            auto tempQueue = iterator->expressions;
            if(!insert_expression_data(tempQueue,  itemsCount, bufferSizes))
            {
                registry->failureReason = INSERTION_FAIL;
                return false;
            }
            sourceBufferSizes.dataBufferSize = bufferSizes.dataBufferSize < sourceBufferSizes.dataBufferSize? bufferSizes.dataBufferSize: sourceBufferSizes.dataBufferSize;
            registry->expressionsDataBuffer  = stapinlogic::t_rpc_array{(uint8_t*)malloc(sourceBufferSizes.dataBufferSize)};
            if(nullptr == registry->expressionsDataBuffer)
            {
                registry->failureReason = ALLOCATION_FAIL;
                return false;
            }
            registry->dataBufferCapacity = sourceBufferSizes.dataBufferSize;
        }
        
        return fill_common_attributes_registry(registry, sourceBufferSizes, bufferSizes);
    }  

    /**
     * @brief notifies rpc message that something failed in the procces.
     * @tparam RES_TYPE type of the return value.
     * @param res Pointer to the Registry structure to be initialized.
     * @param indexes the indexes of the rpc args.
     * @param rpcArgs A pointer to t_rpc_arg. 
     * @param retRpcArg A pointer t_rpc_ret, the rpc return arg we want to assign to.
     * @param returnSchema The return schema to assign. 
     */
    template<typename RES_TYPE>
    void notify_message_fail(RES_TYPE res, const std::set<int>& indexes, t_rpc_arg* rpcArgs, t_rpc_ret* retRpcArg,  t_rpc_arg_schema returnSchema, const std::string& reason)
    {
        //TODO write to log the reason!
        mark_in_paramter_rpc_args(indexes, rpcArgs);
        fill_return_rpc_arg(retRpcArg, returnSchema, res);
    }

} //anonymus namespace

//***************START OF DO RPC ***************** */
void do_rpc(IPindPlugin* self, t_rpc_id rpcId, t_arg_count argCount, t_rpc_arg* rpcArgs, t_rpc_ret* retRpcArg){
      switch (rpcId){
        case  stapin::RPCID_STAPIN_INIT:
        {
            auto pid = static_cast<NATIVE_TID>(rpcArgs[0].argData);
            auto res = tcfandcontextlogic::TCF_Init(pid);
            mark_in_paramter_rpc_args({0}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_INIT_SCHEMA.returnValueSchema, res);
            break;
        }
        case stapin::RPCID_STAPIN_IMAGE_LOAD:
        {
            auto imageName = reinterpret_cast<char*>(rpcArgs[1].argData);
            auto sectionNames = reinterpret_cast<char*>(rpcArgs[2].argData);
            auto RPCSections = reinterpret_cast<stapin::Rpc_section*>(rpcArgs[3].argData);
            auto numSections = static_cast<size_t>(rpcArgs[4].argData);
            auto res = false;
            auto sectionNamesSize = rpcArgs[2].argDataSize;
            if(numSections*sizeof(stapin::Rpc_section) > rpcArgs[3].argDataSize)
            {
                notify_message_fail(res, {0,1,2,3,4}, rpcArgs, retRpcArg, stapin::STAPIN_IMAGE_LOAD_SCHEMA.returnValueSchema, BUFFER_SIZE_FAIL);
                break;
            }
            else if(0 == sectionNamesSize)
            {
                notify_message_fail(res, {0,1,2,3,4}, rpcArgs, retRpcArg, stapin::STAPIN_IMAGE_LOAD_SCHEMA.returnValueSchema, SECTIONS_NAMES_SIZE_ZERO );
                break;
            }
            else if(0 == rpcArgs[1].argDataSize) ///check image name size
            {
                notify_message_fail(res, {0,1,2,3,4}, rpcArgs, retRpcArg, stapin::STAPIN_IMAGE_LOAD_SCHEMA.returnValueSchema, IMAGE_NAME_SIZE_ZERO);
                break;
            }
            else
            {
                res = imagelogic::insert_sections_into_list(imageName,  sectionNames, RPCSections , numSections, sectionNamesSize);
            }
            mark_in_paramter_rpc_args({0,1,2,3,4}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_IMAGE_LOAD_SCHEMA.returnValueSchema, res);
            break;
        }
        case stapin::RPCID_STAPIN_IMAGE_UNLOAD:
        {
            auto imageName = reinterpret_cast<char*>(rpcArgs[1].argData);
            auto res = imagelogic::delete_image_sections_from_sections_list(imageName);
            mark_in_paramter_rpc_args({0,1}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_IMAGE_UNLOAD_SCHEMA.returnValueSchema, res);
            break;
        }
        case stapin::RPCID_STAPIN_THREAD_START:
        {
            auto pid =static_cast<pid_t>(rpcArgs[0].argData);
            auto tid =static_cast<pid_t>(rpcArgs[1].argData);
            auto rpcContext = reinterpret_cast<stapin::RPC_Full_Context*>(rpcArgs[2].argData);
            auto addedThread = stapincontext::add_thread_tcf_context(pid, tid);
            auto updatedRpcContext = false;
            if(addedThread)
            {
                updatedRpcContext = tcfandcontextlogic::update_rpc_context(pid, tid, rpcContext, stapin::FULL_RPC_CONTEXT_SIZE);
            }
            else
            {
                notify_message_fail(addedThread, {0,1,2}, rpcArgs, retRpcArg, stapin::STAPIN_THREAD_START_SCHEMA.returnValueSchema, ADD_THREAD_FAIL);
                break;
            }
            if(!updatedRpcContext)
            {
                notify_message_fail(updatedRpcContext, {0,1,2}, rpcArgs, retRpcArg, stapin::STAPIN_THREAD_START_SCHEMA.returnValueSchema, UPDATE_RPC_CONTEXT_FAIL);
                break;
            }
            auto res = addedThread && updatedRpcContext;
            mark_in_paramter_rpc_args({0,1,2}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_THREAD_START_SCHEMA.returnValueSchema, res);
            break;
        }
        case stapin::RPCID_STAPIN_THREAD_FINI:
        {   
            
            auto pid = static_cast<pid_t>(rpcArgs[0].argData);
            auto tid = static_cast<pid_t>(rpcArgs[1].argData);
            auto res = stapincontext::remove_thread_tcf_context(pid, tid);
            mark_in_paramter_rpc_args({0,1}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_THREAD_FINI_SCHEMA.returnValueSchema, res);
            break;
        }
        case stapin::RPCID_STAPIN_FORK_IN_CHILD :
        {
            auto newPID = static_cast<NATIVE_TID>(rpcArgs[0].argData);
            auto oldPID = static_cast<NATIVE_TID>(rpcArgs[1].argData);
            auto rpcContext = reinterpret_cast<stapin::RPC_Full_Context*>(rpcArgs[2].argData);
            auto res = tcfandcontextlogic::notify_fork_in_child(newPID, oldPID, rpcContext);
            mark_in_paramter_rpc_args({0,1,2}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_FORK_IN_CHILD_SCHEMA.returnValueSchema, res);
            break;
        }
        case stapin::RPCID_STAPIN_SET_CONTEXT:
        {
            auto pid = static_cast<NATIVE_TID>(rpcArgs[0].argData);
            auto tid = static_cast<NATIVE_TID>(rpcArgs[1].argData);
            auto rpcContext = reinterpret_cast<stapin::RPC_Full_Context*>(rpcArgs[2].argData);
            auto size = static_cast<uint16_t>(rpcArgs[3].argData);
            auto res = tcfandcontextlogic::update_rpc_context(pid, tid, rpcContext, size);
            mark_in_paramter_rpc_args({0,1,2,3}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_SET_CONTEXT_SCHEMA.returnValueSchema, res);
            break;
        }
        case stapin::RPCID_STAPIN_SET_IP:
        {
            
            auto pid = static_cast<NATIVE_TID>(rpcArgs[0].argData);
            auto tid = static_cast<NATIVE_TID>(rpcArgs[1].argData);
            auto IP = reinterpret_cast<uint64_t>(&rpcArgs[2].argData);
            auto res = tcfandcontextlogic::update_IP(pid, tid, IP);
            mark_in_paramter_rpc_args({0,1,2}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_SET_IP_SCHEMA.returnValueSchema, res);
            break;
        }  
        case stapin::RPCID_STAPIN_GET_SOURCE_LOCATIONS:
        {
            auto pid = static_cast<NATIVE_TID>(rpcArgs[0].argData);
            auto startAddress = reinterpret_cast<ADDRINT>(rpcArgs[1].argData); //IN
            auto endAddress = reinterpret_cast<ADDRINT>(rpcArgs[2].argData); // IN
            auto sourceFilesNamesSize = (uint16_t)(rpcArgs[4].argData); // IN, OUT
            auto locCount =(uint16_t)(rpcArgs[6].argData); // IN

            //*************handle first buffer*************
            auto sourceNamesbufferSize = sourceFilesNamesSize < stapinlogic::MAX_SOURCE_FILE_NAMES_BUFFER_SIZE? sourceFilesNamesSize: stapinlogic::MAX_SOURCE_FILE_NAMES_BUFFER_SIZE;
           
            //*************handle secone buffer*************Source_loc_tracker
            auto maxClientLocationSize = locCount*sizeof(stapin::Rpc_source_location); // the wanted 
            auto locationsBufferSize = maxClientLocationSize < stapinlogic::MAX_LOCATIONS_BUFFER_SIZE? maxClientLocationSize: stapinlogic::MAX_LOCATIONS_BUFFER_SIZE;

            //*************building the Tracker*************
            stapinlogic::Source_loc_tracker locationsTracker{0};

            locationsTracker.pid = pid;
            locationsTracker.tid = pid;

            locationsTracker.curIndex = 0;
            locationsTracker.startaddress = startAddress;
            locationsTracker.endAddress = endAddress;
            locationsTracker.locationsBuffer = stapinlogic::t_location_array{(stapin::Rpc_source_location*)malloc( locationsBufferSize)};
            locationsTracker.SourceFileNames  = stapinlogic::t_char_array{(char*)malloc(sourceNamesbufferSize)};
            if(nullptr == locationsTracker.locationsBuffer || nullptr == locationsTracker.SourceFileNames)
            {
                notify_message_fail(0, {0,1,2,6}, rpcArgs, retRpcArg, stapin::STAPIN_GET_SOURCE_LOCATIONS_SCHEMA.returnValueSchema, ALLOCATION_FAIL);
                break;
            }
            locationsTracker.locCount = locCount;
            locationsTracker.fileNameIndex = 0;
            locationsTracker.fileNameCapacity = sourceNamesbufferSize;

            //*************call the get source location function *************
            auto res = sourcelocationlogic::get_source_location(&locationsTracker);

            //**********first buffer****************** */
            fill_rpc_buffer(rpcArgs[3], (uint64_t)locationsTracker.SourceFileNames.release(), 
                            stapin::STAPIN_GET_SOURCE_LOCATIONS_SCHEMA.argSchemaArray[3],
                            sourceNamesbufferSize);
           
            //**********second buffer****************** */
            fill_rpc_buffer(rpcArgs[5], (uint64_t)locationsTracker.locationsBuffer.release(), 
                            stapin::STAPIN_GET_SOURCE_LOCATIONS_SCHEMA.argSchemaArray[5],
                            locationsBufferSize);
    
            //*************rest of the arguments including return value *************
            rpcArgs[4].argData = (uint64_t)sourceNamesbufferSize;

            mark_in_paramter_rpc_args({0,1,2,6}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_GET_SOURCE_LOCATIONS_SCHEMA.returnValueSchema, res);
            break;
        }
        case stapin::RPCID_STAPIN_GET_SYMBOLS_START:
        {
            auto pid = static_cast<NATIVE_TID>(rpcArgs[0].argData);
            auto tid = static_cast<NATIVE_TID>(rpcArgs[1].argData);
            auto parentUinqueID =  reinterpret_cast<char*>(rpcArgs[2].argData);
            auto symbolNameToSearch =  reinterpret_cast<char*>(rpcArgs[3].argData);
            auto clientSymbolsNamesBufferSize = (uint16_t)(rpcArgs[5].argData); 
            auto clientUniqueIDSBufferSize = (uint16_t)(rpcArgs[7].argData); 
            auto wantedSymbolsCount = (uint16_t)(rpcArgs[9].argData); 

            Buffer_sizes sourceBufferSizes {0};
        
            //****handle the names buffer ******* */
            //not auto because it is in/out. 
            sourceBufferSizes.itemNamesbufferSize = clientSymbolsNamesBufferSize < stapinlogic::MAX_SYMBOLS_NAMES_BUFFER_SIZE? clientSymbolsNamesBufferSize: stapinlogic::MAX_SYMBOLS_NAMES_BUFFER_SIZE;
        
            //****handle the uinque Ids buffer ******* */
            sourceBufferSizes.uinqueIDSbufferSize = clientUniqueIDSBufferSize < stapinlogic::MAX_UINQUE_IDS_BUFFER_SIZE? clientUniqueIDSBufferSize: stapinlogic::MAX_UINQUE_IDS_BUFFER_SIZE;
          
            //****handle the Rpc_symbol structures buffer ******* */
            auto wantedSymbolsBufferSize = wantedSymbolsCount*sizeof(stapin::Rpc_symbol);
            sourceBufferSizes.itemsBufferSize = wantedSymbolsBufferSize < stapinlogic::MAX_RPC_SYMBOLS_BUFFER? wantedSymbolsBufferSize: stapinlogic::MAX_RPC_SYMBOLS_BUFFER;
         
            //*************building the Registry*************
            stapinlogic::Registry symbolsRegistry{0};
        
            //*************call the get symbols start function *************
            auto curSymbolIterator = symbolslogic::get_symbols_start(parentUinqueID , pid ,tid, symbolNameToSearch);

            if(!init_registry(&symbolsRegistry, sourceBufferSizes, 
                                wantedSymbolsCount, curSymbolIterator))
            {
                notify_message_fail(-1, {0,1,2,3}, rpcArgs, retRpcArg, stapin::STAPIN_GET_SYMBOLS_START_SCHEMA.returnValueSchema, symbolsRegistry.failureReason);
                break;
            }
            auto foundSymbols = symbolslogic::symbols_to_RPC_symbol(wantedSymbolsCount, curSymbolIterator, &symbolsRegistry);
            rpcArgs[5].argData = (uint64_t)sourceBufferSizes.itemNamesbufferSize;
            rpcArgs[7].argData = (uint64_t)sourceBufferSizes.uinqueIDSbufferSize;
            rpcArgs[9].argData = (uint64_t)foundSymbols;

            //****handle the names buffer ******* */
            fill_rpc_buffer(rpcArgs[4], (uint64_t)symbolsRegistry.names.release(), 
                            stapin::STAPIN_GET_SYMBOLS_START_SCHEMA.argSchemaArray[4],
                            sourceBufferSizes.itemNamesbufferSize);

            //****handle the uinque Ids buffer ******* */
            fill_rpc_buffer(rpcArgs[6], (uint64_t)symbolsRegistry.uniqueIDS.release(), 
                            stapin::STAPIN_GET_SYMBOLS_START_SCHEMA.argSchemaArray[6],
                            sourceBufferSizes.uinqueIDSbufferSize);

            //****handle the Rpc_symbol structures buffer ******* */
            fill_rpc_buffer(rpcArgs[8], (uint64_t)symbolsRegistry.rpcItemsArray.release(), 
                            stapin::STAPIN_GET_SYMBOLS_START_SCHEMA.argSchemaArray[8],
                            sourceBufferSizes.itemsBufferSize);

            mark_in_paramter_rpc_args({0,1,2,3}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_GET_SYMBOLS_START_SCHEMA.returnValueSchema, curSymbolIterator->id);
            break;
        }
        case stapin::RPCID_STAPIN_GET_SYMBOLS_NEXT:
        {
            auto iteratorID = (uintptr_t)(rpcArgs[0].argData);
            auto clientSymbolsNamesBufferSize = (uint16_t)(rpcArgs[2].argData); 
            auto clientUniqueIDSBufferSize = (uint16_t)(rpcArgs[4].argData); 
            auto wantedSymbolsCount = (uint16_t)(rpcArgs[6].argData); 

            Buffer_sizes sourceBufferSizes {0};

             //****handle the names buffer ******* */
            sourceBufferSizes.itemNamesbufferSize = clientSymbolsNamesBufferSize < stapinlogic::MAX_SYMBOLS_NAMES_BUFFER_SIZE? clientSymbolsNamesBufferSize: stapinlogic::MAX_SYMBOLS_NAMES_BUFFER_SIZE;
           
            //****handle the uinque Ids buffer ******* */
           sourceBufferSizes.uinqueIDSbufferSize = clientUniqueIDSBufferSize < stapinlogic::MAX_UINQUE_IDS_BUFFER_SIZE? clientUniqueIDSBufferSize: stapinlogic::MAX_UINQUE_IDS_BUFFER_SIZE;
      
            //****handle the Rpc_symbol structures buffer ******* */
            auto wantedSymbolsBufferSize = wantedSymbolsCount*sizeof(stapin::Rpc_symbol);
            sourceBufferSizes.itemsBufferSize = wantedSymbolsBufferSize < stapinlogic::MAX_RPC_SYMBOLS_BUFFER? wantedSymbolsBufferSize: stapinlogic::MAX_RPC_SYMBOLS_BUFFER;
        
            //*************building the symbolsRegistry*************
            stapinlogic::Registry symbolsRegistry{0};

            auto curIterator = symbolslogic::get_symbol_iterator_by_id(iteratorID);
            if(nullptr == curIterator || !init_registry(&symbolsRegistry, sourceBufferSizes, wantedSymbolsCount, curIterator))
            {
                notify_message_fail(false, {0}, rpcArgs, retRpcArg, stapin::STAPIN_GET_SYMBOLS_NEXT_SCHEMA.returnValueSchema, symbolsRegistry.failureReason);
                break;
            }
            auto foundSymbols = symbolslogic::symbols_to_RPC_symbol(wantedSymbolsCount, curIterator, &symbolsRegistry);
            auto found = foundSymbols > 0;

            rpcArgs[2].argData = sourceBufferSizes.itemNamesbufferSize ;
            rpcArgs[4].argData = sourceBufferSizes.uinqueIDSbufferSize;
            rpcArgs[6].argData = foundSymbols;

            /****handle the names buffer ******* */
            fill_rpc_buffer(rpcArgs[1], (uint64_t)symbolsRegistry.names.release(),
                            stapin::STAPIN_GET_SYMBOLS_NEXT_SCHEMA.argSchemaArray[1],
                            sourceBufferSizes.itemNamesbufferSize);

            //****handle the uinque Ids buffer ******* */
             fill_rpc_buffer(rpcArgs[3], (uint64_t)symbolsRegistry.uniqueIDS.release(), 
                            stapin::STAPIN_GET_SYMBOLS_NEXT_SCHEMA.argSchemaArray[3],
                            sourceBufferSizes.uinqueIDSbufferSize);

            //****handle the Rpc_symbol structures buffer ******* */
            fill_rpc_buffer(rpcArgs[5], (uint64_t)symbolsRegistry.rpcItemsArray.release(), 
                            stapin::STAPIN_GET_SYMBOLS_NEXT_SCHEMA.argSchemaArray[5],
                            sourceBufferSizes.itemsBufferSize);

            mark_in_paramter_rpc_args({0}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_GET_SYMBOLS_NEXT_SCHEMA.returnValueSchema, found);
            break;
        }
        case stapin::RPCID_STAPIN_GET_SYMBOLS_END :
        {
            auto iteratorID = reinterpret_cast<uintptr_t>(rpcArgs[0].argData);
            auto res = symbolslogic::remove_symbols_iterator(iteratorID);
            mark_in_paramter_rpc_args({0}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_GET_SYMBOLS_END_SCHEMA.returnValueSchema, res);
            break;
        }
        case stapin::RPCID_STAPIN_GET_SYMBOL_BY_ADDR:
        {
            stapinlogic::Symbol_by_query iteratorByAddress{0};
            iteratorByAddress.pid = static_cast<NATIVE_TID>(rpcArgs[0].argData);
            iteratorByAddress.tid = static_cast<NATIVE_TID>(rpcArgs[1].argData); 
            iteratorByAddress.symbolSearchAddr = reinterpret_cast<ADDRINT>(rpcArgs[2].argData); ;
            iteratorByAddress.resultSymbol = stapinlogic::t_rpc_array{(uint8_t*)calloc(1, sizeof(stapin::Rpc_symbol))}; //set 1 symbol values to deafult values.
            auto resultSymbol = (stapin::Rpc_symbol*)iteratorByAddress.resultSymbol.get();
            resultSymbol->memoryOrReg =  reinterpret_cast<ADDRINT>(rpcArgs[2].argData); 
            symbolslogic::get_symbol_by_address(&iteratorByAddress);
            Symbol_query_params params{
                iteratorByAddress, //iterator
                (uint16_t)(rpcArgs[4].argData), // wantedNameSize
                (uint16_t)(rpcArgs[6].argData), // wantedUniqueIDSize
                rpcArgs, //rpcArgs
                retRpcArg, //retRpcArg
                stapin::STAPIN_GET_SYMBOL_BY_ADDR_SCHEMA // schema
            };
            if(!handle_symbol_query(params))
            {
                notify_message_fail(false, {0,1,2}, rpcArgs, retRpcArg, stapin::STAPIN_GET_SYMBOL_BY_ADDR_SCHEMA.returnValueSchema, params.failureReason);
                break;
            }
            mark_in_paramter_rpc_args({0,1,2}, rpcArgs);
            break;
        }
        case stapin::RPCID_STAPIN_GET_SYMBOL_BY_REG:
        {
            stapinlogic::Symbol_by_query itertorByReg{0};
            itertorByReg.pid = static_cast<NATIVE_TID>(rpcArgs[0].argData);
            itertorByReg.tid = static_cast<NATIVE_TID>(rpcArgs[1].argData); 
            itertorByReg.resultSymbol = stapinlogic::t_rpc_array{(uint8_t*)calloc(1, sizeof(stapin::Rpc_symbol))}; //set 1 symbol values to deafult values.
            auto regIndex = static_cast<unsigned>(rpcArgs[2].argData);
            if(regIndex >= stapinlogic::reg_index_to_name_size())
            {
                notify_message_fail(false, {0,1,2}, rpcArgs, retRpcArg, stapin::STAPIN_GET_SYMBOL_BY_REG_SCHEMA.returnValueSchema, REG_INDEX_FAIL);
                break;
            }
            auto regName = stapinlogic::reg_index_to_name()[regIndex];
            itertorByReg.regName = regName;
            itertorByReg.regIndex = regIndex;
            symbolslogic::get_symbol_by_reg(&itertorByReg);
            Symbol_query_params params{
                itertorByReg,
                (uint16_t)(rpcArgs[4].argData), // wantedNameSize
                (uint16_t)(rpcArgs[6].argData), // wantedUniqueIDSize
                rpcArgs,
                retRpcArg,
                stapin::STAPIN_GET_SYMBOL_BY_REG_SCHEMA
            };
            if(!handle_symbol_query(params))
            {
                notify_message_fail(false, {0,1,2}, rpcArgs, retRpcArg, stapin::STAPIN_GET_SYMBOL_BY_ADDR_SCHEMA.returnValueSchema, params.failureReason);
                break;
            }
            mark_in_paramter_rpc_args({0,1,2}, rpcArgs);
            break;
        }
        case stapin::RPCID_STAPIN_GET_SYMBOL_BY_ID:
        {
            stapinlogic::Symbol_by_query itertorByID{0};
            itertorByID.pid = static_cast<NATIVE_TID>(rpcArgs[0].argData);
            itertorByID.tid = static_cast<NATIVE_TID>(rpcArgs[1].argData);
            itertorByID.resultSymbol = stapinlogic::t_rpc_array{(uint8_t*)calloc(1, sizeof(stapin::Rpc_symbol))}; //set 1 symbol values to deafult values.
            itertorByID.uniqueID =  reinterpret_cast<char*>(rpcArgs[2].argData); 
            symbolslogic::get_symbol_by_id(&itertorByID);
            Symbol_query_params params{
                itertorByID,
                (uint16_t)(rpcArgs[4].argData), // wantedNameSize
                (uint16_t)(rpcArgs[6].argData), // wantedUniqueIDSize
                rpcArgs,
                retRpcArg,
                stapin::STAPIN_GET_SYMBOL_BY_ID_SCHEMA
            };
            if(!handle_symbol_query(params))
            {
                notify_message_fail(false, {0,1,2}, rpcArgs, retRpcArg, stapin::STAPIN_GET_SYMBOL_BY_ADDR_SCHEMA.returnValueSchema, params.failureReason);
                break;
            }
            mark_in_paramter_rpc_args({0,1,2}, rpcArgs);
            break;
        }
        case stapin::RPCID_STAPIN_GET_EXPRESSION_START:
        {
            auto pid = static_cast<NATIVE_TID>(rpcArgs[0].argData);
            auto tid = static_cast<NATIVE_TID>(rpcArgs[1].argData);
            auto expression =  reinterpret_cast<char*>(rpcArgs[2].argData);
            auto clientExpressionsNamesBufferSize = (uint16_t)(rpcArgs[4].argData); 
            auto clientUniqueIDSBufferSize = (uint16_t)(rpcArgs[6].argData); 
            auto wantedExpressionsCount = (uint16_t)(rpcArgs[8].argData); 
            auto wantedExpressionDataSize =  (uint16_t)(rpcArgs[10].argData); 

            Buffer_sizes sourceBufferSizes {0};

            //****handle the names buffer ******* */
            sourceBufferSizes.itemNamesbufferSize = clientExpressionsNamesBufferSize < stapinlogic::MAX_EXPRESSION_NAMES_BUFFER_SIZE? clientExpressionsNamesBufferSize: stapinlogic::MAX_EXPRESSION_NAMES_BUFFER_SIZE;
        
            //****handle the uinque Ids buffer ******* */
            sourceBufferSizes.uinqueIDSbufferSize = clientUniqueIDSBufferSize < stapinlogic::MAX_UINQUE_IDS_BUFFER_SIZE? clientUniqueIDSBufferSize: stapinlogic::MAX_UINQUE_IDS_BUFFER_SIZE;
          
            //****handle the Rpc_expression structures buffer ******* */
            auto wantedExpressionsBufferSize = wantedExpressionsCount*(sizeof(stapin::Rpc_expression) + MAX_EXPRESSION_DATA_SIZE);
            sourceBufferSizes.itemsBufferSize = wantedExpressionsBufferSize < stapinlogic::MAX_RPC_EXPRESSIONS_BUFFER? wantedExpressionsBufferSize: stapinlogic::MAX_RPC_EXPRESSIONS_BUFFER;

            /*  handle the rpc expression data buffer */
            sourceBufferSizes.dataBufferSize = wantedExpressionDataSize < stapinlogic::MAX_EXPRESSION_DATA_BUFFER_SIZE? wantedExpressionDataSize: stapinlogic::MAX_EXPRESSION_DATA_BUFFER_SIZE;
         
            //*************building the Registry*************
            stapinlogic::Registry expressionRegistry{0};

            //*************call the get symbols start function *************
            auto resultIterator = expressionslogic::get_expressions_start(pid, tid, std::string(expression));
            if(!init_registry(&expressionRegistry, sourceBufferSizes,  wantedExpressionsCount, resultIterator))
            {
                notify_message_fail(-1, {0,1,2}, rpcArgs, retRpcArg, stapin::STAPIN_GET_EXPRESSION_START_SCHEMA.returnValueSchema, expressionRegistry.failureReason);
                break;
            }
            auto foundExpressions = expressionslogic::expressions_to_RPC_expressions(wantedExpressionsCount, resultIterator, &expressionRegistry);

            rpcArgs[4].argData = (uint64_t)sourceBufferSizes.itemNamesbufferSize;
            rpcArgs[6].argData = (uint64_t)sourceBufferSizes.uinqueIDSbufferSize;
            rpcArgs[8].argData = (uint64_t)foundExpressions;
            rpcArgs[10].argData = (uint64_t)sourceBufferSizes.dataBufferSize;

            //****handle the names buffer ******* */
            fill_rpc_buffer(rpcArgs[3], (uint64_t)expressionRegistry.names.release(), 
                            stapin::STAPIN_GET_EXPRESSION_START_SCHEMA.argSchemaArray[3],
                            sourceBufferSizes.itemNamesbufferSize);

            //****handle the uinque Ids buffer ******* */
            fill_rpc_buffer(rpcArgs[5], (uint64_t)expressionRegistry.uniqueIDS.release(), 
                            stapin::STAPIN_GET_EXPRESSION_START_SCHEMA.argSchemaArray[5],
                            sourceBufferSizes.uinqueIDSbufferSize);

            //****handle the Rpc_expressions structures buffer ******* */
            fill_rpc_buffer(rpcArgs[7], (uint64_t)expressionRegistry.rpcItemsArray.release(), 
                            stapin::STAPIN_GET_EXPRESSION_START_SCHEMA.argSchemaArray[7],
                            sourceBufferSizes.itemsBufferSize);
            
            //****handle the data buffer ******* */
            fill_rpc_buffer(rpcArgs[9], (uint64_t)expressionRegistry.expressionsDataBuffer.release(), 
                            stapin::STAPIN_GET_EXPRESSION_START_SCHEMA.argSchemaArray[9],
                            sourceBufferSizes.dataBufferSize);

            mark_in_paramter_rpc_args({0,1,2}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_GET_EXPRESSION_START_SCHEMA.returnValueSchema, resultIterator->id);
            break;
        }
        case stapin::RPCID_STAPIN_GET_EXPRESSION_NEXT:
        {
            auto iteratorID = (uintptr_t)(rpcArgs[0].argData);
            auto clientExpressionsNamesBufferSize = (uint16_t)(rpcArgs[2].argData); 
            auto clientUniqueIDSBufferSize = (uint16_t)(rpcArgs[4].argData); 
            auto wantedExpressionsCount = (uint16_t)(rpcArgs[6].argData); 
            auto wantedExpressionDataSize =  (uint16_t)(rpcArgs[8].argData); 

            Buffer_sizes sourceBufferSizes {0};

            //****handle the names buffer ******* */
            sourceBufferSizes.itemNamesbufferSize = clientExpressionsNamesBufferSize < stapinlogic::MAX_EXPRESSION_NAMES_BUFFER_SIZE? clientExpressionsNamesBufferSize: stapinlogic::MAX_EXPRESSION_NAMES_BUFFER_SIZE;
        
            //****handle the uinque Ids buffer ******* */
            sourceBufferSizes.uinqueIDSbufferSize = clientUniqueIDSBufferSize < stapinlogic::MAX_UINQUE_IDS_BUFFER_SIZE? clientUniqueIDSBufferSize: stapinlogic::MAX_UINQUE_IDS_BUFFER_SIZE;
          
            //****handle the Rpc_symbol structures buffer ******* */
            auto wantedExpressionsBufferSize = wantedExpressionsCount*(sizeof(stapin::Rpc_expression) + MAX_EXPRESSION_DATA_SIZE);
            sourceBufferSizes.itemsBufferSize = wantedExpressionsBufferSize < stapinlogic::MAX_RPC_EXPRESSIONS_BUFFER? wantedExpressionsBufferSize: stapinlogic::MAX_RPC_EXPRESSIONS_BUFFER;
         
            /*  handle the rpc expression data buffer */
            sourceBufferSizes.dataBufferSize = wantedExpressionDataSize < stapinlogic::MAX_EXPRESSION_DATA_BUFFER_SIZE? wantedExpressionDataSize: stapinlogic::MAX_EXPRESSION_DATA_BUFFER_SIZE;
            
            //*************building the Registry*************
            stapinlogic::Registry expressionRegistry{0};

            //*************call the get symbols start function *************
            auto resultIterator = expressionslogic::get_expressions_iterator_by_id(iteratorID);
            if(nullptr == resultIterator || !init_registry(&expressionRegistry, sourceBufferSizes, wantedExpressionsCount,resultIterator )) 
            {
                notify_message_fail(false, {0}, rpcArgs, retRpcArg, stapin::STAPIN_GET_EXPRESSION_NEXT_SCHEMA.returnValueSchema, expressionRegistry.failureReason);
                break;
            }
    
            auto foundExpressions = expressionslogic::expressions_to_RPC_expressions(wantedExpressionsCount, resultIterator, &expressionRegistry);
            auto found = foundExpressions > 0;


            rpcArgs[2].argData = (uint64_t)sourceBufferSizes.itemNamesbufferSize;
            rpcArgs[4].argData = (uint64_t)sourceBufferSizes.uinqueIDSbufferSize;
            rpcArgs[6].argData = (uint64_t)foundExpressions;
            rpcArgs[8].argData = (uint64_t)sourceBufferSizes.dataBufferSize;

            //****handle the names buffer ******* */
            fill_rpc_buffer(rpcArgs[1], (uint64_t)expressionRegistry.names.release(), 
                            stapin::STAPIN_GET_EXPRESSION_NEXT_SCHEMA.argSchemaArray[1],
                            sourceBufferSizes.itemNamesbufferSize);

            //****handle the uinque Ids buffer ******* */
            fill_rpc_buffer(rpcArgs[3], (uint64_t)expressionRegistry.uniqueIDS.release(), 
                            stapin::STAPIN_GET_EXPRESSION_NEXT_SCHEMA.argSchemaArray[3],
                            sourceBufferSizes.uinqueIDSbufferSize);

            //****handle the Rpc_symbol structures buffer ******* */
            fill_rpc_buffer(rpcArgs[5], (uint64_t)expressionRegistry.rpcItemsArray.release(), 
                            stapin::STAPIN_GET_EXPRESSION_NEXT_SCHEMA.argSchemaArray[5],
                            sourceBufferSizes.itemsBufferSize);

            //****handle the data buffer ******* */
            fill_rpc_buffer(rpcArgs[7], (uint64_t)expressionRegistry.expressionsDataBuffer.release(), 
                            stapin::STAPIN_GET_EXPRESSION_START_SCHEMA.argSchemaArray[7],
                            sourceBufferSizes.dataBufferSize);

            mark_in_paramter_rpc_args({0}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_GET_EXPRESSION_NEXT_SCHEMA.returnValueSchema, found);
            break;
        }
        case stapin::RPCID_STAPIN_GET_EXPRESSION_END:
        {
            auto iteratorID = reinterpret_cast<uintptr_t>(rpcArgs[0].argData);
            auto res = expressionslogic::remove_expressions_iterator(iteratorID);
            mark_in_paramter_rpc_args({0}, rpcArgs);
            fill_return_rpc_arg(retRpcArg, stapin::STAPIN_GET_EXPRESSION_END_SCHEMA.returnValueSchema, res);
            break;
        }
      }
}


//**************************THE MUST HAVE FUNCTIONS FOR PLUGIN*********************************** */

t_rpc_message_schema const* get_rpc_schema(IPindPlugin* self, t_rpc_id rpcId){
    switch (rpcId)
    {
    case  stapin::RPCID_STAPIN_INIT:
        return &stapin::STAPIN_INIT_SCHEMA;
    case stapin::RPCID_STAPIN_IMAGE_LOAD:
        return &stapin::STAPIN_IMAGE_LOAD_SCHEMA;
    case stapin::RPCID_STAPIN_IMAGE_UNLOAD:
        return &stapin::STAPIN_IMAGE_UNLOAD_SCHEMA;
    case stapin::RPCID_STAPIN_THREAD_START:
        return &stapin::STAPIN_THREAD_START_SCHEMA;
    case stapin::RPCID_STAPIN_THREAD_FINI:
        return &stapin::STAPIN_THREAD_FINI_SCHEMA;
    case stapin::RPCID_STAPIN_FORK_IN_CHILD :
        return &stapin::STAPIN_FORK_IN_CHILD_SCHEMA;
    case stapin::RPCID_STAPIN_SET_CONTEXT:
        return &stapin::STAPIN_SET_CONTEXT_SCHEMA;
    case stapin::RPCID_STAPIN_SET_IP:
        return &stapin::STAPIN_SET_IP_SCHEMA;
    case stapin::RPCID_STAPIN_GET_SOURCE_LOCATIONS:
        return &stapin::STAPIN_GET_SOURCE_LOCATIONS_SCHEMA;
    case stapin::RPCID_STAPIN_GET_SYMBOLS_START:
        return &stapin::STAPIN_GET_SYMBOLS_START_SCHEMA;
    case stapin::RPCID_STAPIN_GET_SYMBOLS_NEXT:
        return &stapin::STAPIN_GET_SYMBOLS_NEXT_SCHEMA;
    case stapin::RPCID_STAPIN_GET_SYMBOLS_END :
        return &stapin::STAPIN_GET_SYMBOLS_END_SCHEMA;
    case stapin::RPCID_STAPIN_GET_SYMBOL_BY_ADDR:
        return &stapin::STAPIN_GET_SYMBOL_BY_ADDR_SCHEMA;
    case stapin::RPCID_STAPIN_GET_SYMBOL_BY_REG:
        return &stapin::STAPIN_GET_SYMBOL_BY_REG_SCHEMA;
    case stapin::RPCID_STAPIN_GET_SYMBOL_BY_ID:
        return &stapin::STAPIN_GET_SYMBOL_BY_ID_SCHEMA;
    case stapin::RPCID_STAPIN_GET_EXPRESSION_START:
        return &stapin::STAPIN_GET_EXPRESSION_START_SCHEMA;
    case stapin::RPCID_STAPIN_GET_EXPRESSION_NEXT:
        return &stapin::STAPIN_GET_EXPRESSION_NEXT_SCHEMA;
    case stapin::RPCID_STAPIN_GET_EXPRESSION_END:
        return &stapin::STAPIN_GET_EXPRESSION_END_SCHEMA;
    default:
        break;
    }
    return nullptr;

}

/* 
 * This function should always return RPC.
 */
E_plugin_type get_plugin_type(IPindPlugin* self) { return RPC; }

bool init(IPindPlugin* self,int argc, const char* const argv[]) { return true; }

void uninit(IPindPlugin* self)
{
    exit_event_loop();
    tcfandcontextlogic::wait_for_event_thread();
    return;
}

PLUGIN_EXTERNC PLUGIN__DLLVIS struct IPindPlugin* load_plugin(const char* name)
{
    static IRPCPlugin Self_rpcPlugin;
    static const char* STAPIN_PLUGIN_NAME = "stapin-plugin";

    if (0 != strncmp(STAPIN_PLUGIN_NAME, name, sizeof(STAPIN_PLUGIN_NAME)))
    {
        return nullptr;
    }

    IRPCPlugin* rpcPlugin = &Self_rpcPlugin;
    memset(rpcPlugin, 0, sizeof(IRPCPlugin));
    rpcPlugin->base_.get_plugin_type = get_plugin_type;
    rpcPlugin->base_.init            = init;
    rpcPlugin->base_.uninit          = uninit;
    rpcPlugin->get_rpc_schema        = get_rpc_schema;
    rpcPlugin->do_rpc                = do_rpc;
    return (IPindPlugin*)rpcPlugin;
}

PLUGIN_EXTERNC PLUGIN__DLLVIS void unload_plugin(struct IPindPlugin* plugin) {}
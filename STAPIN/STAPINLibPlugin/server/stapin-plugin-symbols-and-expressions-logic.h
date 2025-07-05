
/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifndef SYMBOLS_AND_EXPRESSIONS_H
#define SYMBOLS_AND_EXPRESSIONS_H

#include "server-utils.h"

#include <condition_variable>
#include <mutex>
#include <string>
#include <queue>
#include <unordered_map>

namespace stapinlogic{
    const std::string CLOSE_ARRAY = "]";
    const std::string OPEN_ARRAY = "[";

    /**
     * @brief A registry structure for managing RPC items, names, and unique IDs.
     * This structure holds various buffers, maps, and synchronization primitives
     * for managing RPC items, names, and unique IDs.
     */
    struct Registry {
        stapinlogic::t_char_array names;                        //!< Array of names
        stapinlogic::t_char_array uniqueIDS;                    //!< Array of unique IDs
        stapinlogic::t_rpc_array expressionsDataBuffer;         //!< Buffer of the data of the expressions
        std::string parentUinqueID;                             //!< Parent unique ID
        std::unordered_map<std::string, uint16_t> name2index;   //!< Map from name to index
        std::unordered_map<std::string, uint16_t> uniqueID2index; //!< Map from unique ID to index
        uint16_t curDataIndex;                                   //!< The current data buffer index
        uint16_t curIndex;                                      //!< Current index in the rpc items.
        uint16_t itemsCount;                                    //!< Count of items
        std::condition_variable cv;                             //!< Condition variable for synchronization
        std::mutex mtx;                                         //!< Mutex for synchronization
        uint16_t curNamesFillingIndex;                          //!< Current filling index for names
        uint16_t curUniqueIDFillingIndex;                       //!< Current filling index for unique IDs
        uint16_t namesBufferCapacity;                           //!< Capacity of the names buffer
        uint16_t uniqueIDsBufferCapacity;                       //!< Capacity of the unique IDs buffer
        uint16_t dataBufferCapacity;                            //!< Capacity of the data buffer
        t_rpc_array rpcItemsArray;                              //!< Array of RPC symbols or expressiopns
        size_t rpcItemsArraySize;                               //!< Size of the rpc items array, to make sure we did not exceeded.
        std::string failureReason;                              //!< Reason of fail if occurred.
    };

    //This is here because both expressions and symbols use this 
    enum class E_rpc_symbol_flags {
        LOCATION_FETCH_ERROR = 0, //!< Error fetching location
        VALUE_IN_MEM = 1,         //!< Value is in memory
        VALUE_IN_REG = 2,         //!< Value is in register
        OPTIMIZED_AWAY = 3,       //!< Value is optimized away
        SYMBOL_IS_TYPE = 4,       //!< Symbol is a type
        DW_STACK_VALUE = 5,       //!< DWARF stack value
        IS_BIT_FIELD = 6          //!< Symbol is a bit field
    };

    // This is here because both expressions and symbols use this 
    struct Symbol_attributes {
        ContextAddress addr;                      //!< Address of the symbol
        ContextAddress size;                      //!< Size of the symbol
        std::string uniqueID;                     //!< Unique ID of the symbol
        std::string name;                         //!< Name of the symbol
        std::string typeUniqueID;                 //!< Unique ID of the type
        std::string baseTypeUniqueID;             //!< Unique ID of the base type
        SYM_FLAGS flags;                          //!< Symbol flags
        int symbolClass;                          //!< Class of the symbol
        stapinlogic::E_rpc_symbol_flags rpcFlags; //!< RPC symbol flags
        uint64_t regIndex;                        //!< Register index
    };

    typedef std::deque<stapinlogic::Symbol_attributes> t_Symbol_attributes_queue;

    // This is here because both expressions and symbols use this 
    struct Symbols_iterator : stapinlogic::Async_args {
        t_Symbol_attributes_queue symbols;                  //!< Queue of delivered symbols (FIFO)
        uintptr_t id;                                       //!< Unique ID of the iterator
        std::string parentUinqueID;                         //!< Parent unique ID
        std::string name;                                   //!< Name of the symbol
    };

    //This is here because both expressions and symbols use this 
    // Typedef for mapping symbol IDs to symbol iterators
    typedef std::unordered_map<uintptr_t, std::shared_ptr<stapinlogic::Symbols_iterator>> SymbolsIDToIteratorMap;
};




namespace symbolsandexpressionslogic{
    //This is here because both expressions and symbols use this 

    /**
     * @brief Retriving the iteratorID to iterator map safely.
     */
    stapinlogic::SymbolsIDToIteratorMap& symbols_id_to_iterator();

    /**
     * @brief Copies name or Unique ID of an expression or symbol into the relevant buffer in registry.
     * @details This function  copies name or unique IDs into  the relevant buffers, and updates
     *          indices and offsets. 
     * @param nameOrID A string which is the name or Unique ID to copy.
     * @param registry A Registry copy the name or Unique ID to.
     * @param isName A boolean indicating wether it is a name or Unique ID to know in which buffer to copy  to.
     * @param isSymbol A boolean to indicate if it is a symbol registry or expression registry. 
     */
    void insert_name_or_ID_to_registry_buffer(const std::string& nameOrID, stapinlogic::Registry* registry, bool isName, bool isSymbol);

    /**
     * @brief Retrieves an iterator by its ID from a map of iterators.
     * This function searches for an iterator with the specified ID in the provided map of iterators.
     * It locks the map using the provided mutex to ensure thread safety during the search.
     * @tparam ITERATOR_TYPE The type of the iterator.
     * @tparam MAP_TYPE The type of the map containing the iterators.
     * @tparam MUTEX_TYPE The type of the mutex used to lock the map.
     * @param iteratorID The ID of the iterator to retrieve.
     * @param iteratorMap The map containing the iterators.
     * @param mapMutex The mutex used to lock the map.
     * @return A pointer to the iterator if found, otherwise nullptr.
     */
    template <typename ITERATOR_TYPE, typename MAP_TYPE, typename MUTEX_TYPE>
    inline ITERATOR_TYPE* get_iterator_by_id(uintptr_t iteratorID, const MAP_TYPE& iteratorMap, MUTEX_TYPE& mapMutex) {
        ITERATOR_TYPE* curIterator = nullptr;
        {
            std::lock_guard<MUTEX_TYPE> lock(mapMutex);
            auto it = iteratorMap.find(iteratorID);
            if (iteratorMap.end() != it) {
                curIterator = it->second.get();
            }
        }
        if(nullptr == curIterator)
        {
            //TODO write to log
        }
        return curIterator;
    }

    //This is here because both expressions and symbols use this 
    /**
     * @brief Function to fill the symbol attributes from given symbol to a given symbol.
     * @details This function retrives symbol attribute such as name, register, address, 
     * size, flags etc and put them in the symbolAttr.
     * @param symbol A pointer to the Symbol structure where the attributes are stored.
     * @param symbolAttr A pointer to the Symbol_attributes structure where the attributes will be stored.
     */
    void fill_symbol_attributes(Symbol *symbol, stapinlogic::Symbol_attributes *symbolAttr);

    /**
     * @brief Function to handle the type name of a symbol.
     * @details This function constructs the type name of a symbol, taking into account
     *          whether the type is a pointer, whether it is const-qualified, whether it is an array
     *          and the number of levels of pointers involved.
     * @param symbol A pointer to the Symbol structure containing symbol details.
     * @param symbolAttr A pointer to the Symbol_attributes structure where the type name will be stored.
     */
    void handle_type_name(Symbol *symbol, stapinlogic::Symbol_attributes *symbolAttr);
};



#endif 
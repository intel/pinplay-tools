/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifndef SYMBOLS_LOGIC_H
#define SYMBOLS_LOGIC_H

#include <array>
#include <string>
#include <unordered_map>

#include "stapin-plugin-symbols-and-expressions-logic.h"

namespace stapinlogic{

    constexpr int SYMBOL_TYPE_FLAGS_MASK = SYM_FLAG_CONST_TYPE | SYM_FLAG_PACKET_TYPE |
                                        SYM_FLAG_SUBRANGE_TYPE | SYM_FLAG_VOLATILE_TYPE |
                                        SYM_FLAG_RESTRICT_TYPE | SYM_FLAG_UNION_TYPE |
                                        SYM_FLAG_CLASS_TYPE | SYM_FLAG_INTERFACE_TYPE |
                                        SYM_FLAG_SHARED_TYPE | SYM_FLAG_ENUM_TYPE |
                                        SYM_FLAG_STRUCT_TYPE | SYM_FLAG_STRING_TYPE |
                                        SYM_FLAG_BOOL_TYPE;

    constexpr uint16_t MAX_SYMBOLS_NAMES_BUFFER_SIZE = 13000;
    constexpr uint16_t MAX_RPC_SYMBOLS_BUFFER = 11000 ; 

    //TYPES
    enum class E_symbol_type {
        UNKNOWN,    //!< Unknown symbol type
        CLASS,      //!< Class type
        FUNCTION,   //!< Function type
        INTEGER,    //!< Integer type
        POINTER,    //!< Pointer type
        REAL,       //!< Real number type
        ARRAY,      //!< Array type
        VALUE       //!< Value type
    };

    struct Symbol_by_query : stapinlogic::Async_args{
        uintptr_t symbolSearchAddr;                //!< Optional, used only in address-based queries
        stapinlogic::E_rpc_symbol_flags flags;     //!< Flags of the symbol
        std::string regName;                       //!< Optional, used only in register-based queries
        int regIndex;                              //!< Optional, used only in ID-based queries
        t_rpc_array resultSymbol;                  //!< Array of result symbols
        std::string uniqueID;                      //!< Unique ID of the symbol
        std::string typeUniqueID;                  //!< Unique ID of the type
        std::string baseTypeUniqueID;              //!< Unique ID of the base type
        std::string symbolName;                    //!< Name of the symbol
        bool found;                                //!< Indicator wether a symbol was found or not.                                         
    };

    const std::string IP_NAME = 
    #if defined(TARGET_IA32E) 
    "rip";
    #else
    "eip";
    #endif
};

namespace symbolslogic
{
    /**
     * @brief Initiates the symbol retrieval process based on the presence of a parent symbol and specific conditions.
     * @details This checks if a parent symbol ID is provided, and either starts the symbol
     *          retrieval process with or without a parent symbol. It waits for the completion of the symbol retrieval
     *          and then updates the symbolsIDToIterator map with a new Symbols_iterator instance.
     * @param parentUinqueID A character pointer to the unique ID of the parent symbol, if any.
     * @param NATIVE_TID pid pid of the process.
     * @param NATIVE_TID tid pid of the calling thread.
     * @param name A character pointer to the name of the symbol to find, if specified.
     * @return The unique identifier of the newly created Symbols_iterator instance.
     */
    stapinlogic::Symbols_iterator* get_symbols_start(const char* parentUinqueID, NATIVE_TID pid, NATIVE_TID tid, const char* name);

    /**
     * @brief Processes a specified number of symbols from a Symbols_iterator and converts them to RPC format.
     * @details This function iteratively processes symbols, converts each to RPC format using symbols_to_RPC_symbol_wrapper,
     *          and manages the count of successfully processed symbols. It stops processing if a conversion fails.
     * @param count The number of symbols to process.
     * @param curSymbolsWrapper A pointer to Registry where the RPC-compatible symbol data is stored.
     * @return The number of symbols successfully processed and converted to RPC format.
     */
    uint16_t symbols_to_RPC_symbol(uint16_t count, stapinlogic::Symbols_iterator* curSymbolsIterator , stapinlogic::Registry* curSymbolsRegistry);

    /**
     * @brief Removes Symbols Iterator by ID.
     * @param iteratorID The Symbols Iterator ID to be removed.
     * @return True if removed succesfully, false otherwise.
     */
    bool remove_symbols_iterator(uintptr_t iteratorID);

    /**
     * @brief Synchronously retrieves a symbol by its address using an asynchronous helper function.
     * @details This function posts an asynchronous event to retrieve the symbol by address,
     *          and waits for the event to complete. 
     * @param curQuery A pointer to Symbol_by_query which contains the address to look for and where results will be stored.
     */
    void get_symbol_by_address(stapinlogic::Symbol_by_query* curQuery);

    /**
     * @brief Synchronously retrieves a symbol by register using an asynchronous helper function.
     * @details This function posts an asynchronous event to retrieve the symbol by register,
     *          and waits for the event to complete. 
     * @param curQuery A pointer to Symbol_by_query which contains the register name and where results will be stored.
     */
    void get_symbol_by_reg(stapinlogic::Symbol_by_query* curQuery);

    /**
     * @brief Synchronously retrieves a symbol by its unique ID using an asynchronous helper function.
     * @details This function posts an asynchronous event to retrieve the symbol by its unique ID,
     *          and waits for the event to complete. 
     * @param curQuery A pointer to Symbol_by_query which contains the unique ID and where results will be stored.
     */
    void get_symbol_by_id(stapinlogic::Symbol_by_query* curQuery);

    /**
     * @brief Get Symbols itertor by ID.
     * @return the result iterator or nullptr if not found.
     */
    stapinlogic::Symbols_iterator* get_symbol_iterator_by_id(uintptr_t iteratorID);
};


#endif
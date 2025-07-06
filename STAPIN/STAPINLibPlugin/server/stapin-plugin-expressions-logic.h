/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifndef EXPRESSIONS_LOGIC_H
#define EXPRESSIONS_LOGIC_H

#include "stapin-plugin-symbols-and-expressions-logic.h"

#include <condition_variable>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include <cstdint>

namespace stapinlogic{
    //To use in stapin-pluin.cpp
    constexpr uint16_t MAX_EXPRESSION_NAMES_BUFFER_SIZE = 13000;
    constexpr uint16_t MAX_RPC_EXPRESSIONS_BUFFER = 13000;
    constexpr uint16_t MAX_EXPRESSION_DATA_BUFFER_SIZE = 64 * 300;

    enum class Expression_type {
        UNKNOWN,        //!< Unknown expression type
        UNSIGNED,       //!< Unsigned integer type
        SIGNED,         //!< Signed integer type
        DOUBLE,         //!< Double precision floating point type
        CLASS,          //!< Class type
        FUNCTION,       //!< Function type
        POINTER,        //!< Pointer type
        ARRAY,          //!< Array type
        ENUM,           //!< Enumeration type
        MEMBER_PTR      //!< Member pointer type
    };

    struct Expression_attributes {
        std::string name;                //!< Name of the expression
        std::string parentName;          //!< Name of the parent expression
        std::string typeUniqueID;        //!< Unique ID of the type
        std::string symbolUniqueID;      //!< Unique ID of the symbol if expression has symbol presentation, else empty
        Expression_type type;            //!< Type of the expression
        int level;                       //!< Level of the expression in the hierarchy
        uint16_t dataSize;               //!< Size of the data
        std::vector<uint8_t> data;       //!< Data associated with the expression
    };

    typedef std::deque<stapinlogic::Expression_attributes> t_Expressions_attributes_queue;

    struct Expression_iterator : stapinlogic::Async_args{
        t_Expressions_attributes_queue expressions; //!< Queue of expression attributes
        uintptr_t id;                               //!< Unique ID of the iterator
        std::string expression;                     //!< Expression string
    };

    typedef std::unordered_map<uintptr_t, std::shared_ptr<stapinlogic::Expression_iterator>> ExpressionsIDToIteratorMap;

};

namespace expressionslogic{
    /**
     * @brief Initiates the expressions retrieval process based on the expression given.
     * @param NATIVE_TID pid pid of the process.
     * @param NATIVE_TID tid pid of the calling thread.
     * @return The newly created Expression_iterator instance.
     */
    stapinlogic::Expression_iterator* get_expressions_start(NATIVE_TID pid, NATIVE_TID tid, const std::string& expression);

    /**
     * @brief Get expressions itertor by ID.
     * @return the result iterator or nullptr if not found.
     */
    stapinlogic::Expression_iterator* get_expressions_iterator_by_id(uintptr_t iteratorID);

    /**
     * @brief Processes a specified number of expressions from a Expression_iterator and converts them to RPC format.
     * @details This function iteratively processes expressions, converts each to RPC format using expressions_to_RPC_expressions_wrapper,
     *          and manages the count of successfully processed expressions. It stops processing if a conversion fails.
     * @param count The number of expressions to process.
     * @param curExpressionsIterator The expressions iterator to look from.
     * @param curSExpressionsRegistry The expressions registry to put the information. 
     * @return The number of expressions successfully processed and converted to RPC format.
     */
    uint16_t expressions_to_RPC_expressions(uint16_t count, stapinlogic::Expression_iterator* curExpressionsIterator , stapinlogic::Registry* curSExpressionsRegistry);


    /**
     * @brief Removes Expressions Iterator by ID.
     * @param iteratorID The Expressions Iterator ID to be removed.
     * @return True if removed succesfully, false otherwise.
     */
    bool remove_expressions_iterator(uintptr_t iteratorID);
};



#endif
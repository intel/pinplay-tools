/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include <stapin_schemas.h>
#include "stapin-context.h"

#include <cstdint>

namespace stapinlogic{
    /**
     * @brief Custom deleter for use with std::unique_ptr to manage memory allocated with malloc.
     * This deleter ensures that memory allocated with malloc is properly freed using free when
     * the std::unique_ptr goes out of scope.
     * @tparam T The type of the pointer to be deleted.
     */
    template<typename T>
    struct Unique_ptr_deleter {
        /**
         * @brief Deletes the pointer by calling free.
         * @param p Pointer to the memory to be freed.
         */
        void operator()(T* p) {
            free((void*)p);
        }
    };

    using t_rpc_array = std::unique_ptr<uint8_t, stapinlogic::Unique_ptr_deleter<uint8_t>>;
    using t_char_array = std::unique_ptr<char, stapinlogic::Unique_ptr_deleter<char>>;
    using t_location_array = std::unique_ptr<stapin::Rpc_source_location, stapinlogic::Unique_ptr_deleter<stapin::Rpc_source_location>>;

    constexpr uint16_t NO_OFFSET = -1;
    constexpr char NULL_TERMINATOR = '\0';

    //IMPLEMENTED in  stapin-plugin-TCF-and-context-logic.cpp
    extern const char** reg_index_to_name();
    extern const size_t reg_index_to_name_size();

    /**
     * @brief Finds the index of given reg.
     * @param regName Name of the register to search.
     * @return the index if found, 0 else.
     */
    uint64_t get_reg_index(const char* regName);

};

#endif

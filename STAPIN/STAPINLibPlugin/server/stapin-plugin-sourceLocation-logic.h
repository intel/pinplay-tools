/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifndef SOURCE_LOCATION_LOGIC_H
#define SOURCE_LOCATION_LOGIC_H

#include "server-utils.h"

#include <cstdint>
#include <condition_variable>

namespace stapinlogic{
    constexpr uint16_t MAX_SOURCE_FILE_NAMES_BUFFER_SIZE = 13000;
    constexpr uint16_t MAX_LOCATIONS_BUFFER_SIZE = 13000;

    struct Source_loc_tracker : stapinlogic::Async_args {
        uintptr_t startaddress;                //!< Start address of the source location
        uintptr_t endAddress;                  //!< End address of the source location
        stapinlogic::t_char_array SourceFileNames; //!< Array of source file names
        stapinlogic::t_location_array locationsBuffer; //!< Buffer of locations
        uint16_t locCount;                     //!< Count of locations
        uint16_t curIndex;                     //!< Current index in the locations buffer
        size_t fileNameIndex;                  //!< Index to track the next position in SourceFileNames
        size_t fileNameCapacity;               //!< Total capacity of SourceFileNames
    };
};

namespace sourcelocationlogic{
    /**
    * @brief
    Retrieves the source location information after the address-to-line mapping process is complete.
    * @details
    This function locks the buffer, posts an event to start the asynchronous 
    address-to-line mapping, and waits for a notification.
    Once notified, it returns the current index of the buffer,
    indicating how many source locations have been filled.
    * @param locationsWrapper A pointer to a user-defined structure that holds 
    the buffer and synchronization primitives.
    * @return
    The current index of the buffer, representing the number of source locations filled. 
    **/
    size_t get_source_location( stapinlogic::Source_loc_tracker *locationsWrapper);
};


#endif
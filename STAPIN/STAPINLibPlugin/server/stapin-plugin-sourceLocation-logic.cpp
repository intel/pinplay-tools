
/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#include "stapin-plugin-sourceLocation-logic.h"

namespace{
    /**
    * @brief
    Callback function that processes line number information for a given code
    area and stores it in a provided buffer.
    * @details
    This function is called during the address-to-line mapping process.
    It gets the current location from the provided buffer, increments the current index,
    and fills the current location with source location information such as file names, 
    start and end addresses, columns, and lines.
    If the buffer is full or an error occurs, the function returns early without updating the current location.
    * @param area A pointer to a CodeArea structure containing the line number information.
    * @param args A void pointer to a user-defined structure that holds the buffer and synchronization primitives. 
    **/
    void line_numbers_callback(CodeArea * area, void * args) {
        auto locationsWrapper = static_cast<stapinlogic::Source_loc_tracker*>(args);
        auto curSourceLocation = &locationsWrapper->locationsBuffer.get()[locationsWrapper->curIndex++];
        if(locationsWrapper->curIndex > locationsWrapper->locCount)
        {
            //TODO write to log
            return; 
        } 
        //*******************fill the source location names ****************************
        // Append the file name to SourceFileNames
        const auto FILENAME_LENGTH_LIMIT = locationsWrapper->fileNameCapacity - locationsWrapper->fileNameIndex;
        auto fileNameLength = strnlen(area->file, FILENAME_LENGTH_LIMIT);
        if(FILENAME_LENGTH_LIMIT == fileNameLength)
        {
            //TODO write to log
            return;
        }
        memcpy(locationsWrapper->SourceFileNames.get() + locationsWrapper->fileNameIndex, area->file, fileNameLength);
        curSourceLocation->sourceFileOffset = locationsWrapper->fileNameIndex;
        locationsWrapper->fileNameIndex += fileNameLength;
        locationsWrapper->SourceFileNames.get()[locationsWrapper->fileNameIndex++] = stapinlogic::NULL_TERMINATOR; // Append null character
        //*******************fill the source location struct****************************
        curSourceLocation->startAddress = (uint64_t)area->start_address;
        curSourceLocation->endAddress = (uint64_t)area->end_address;
        curSourceLocation->startColumn = (int32_t)area->start_column;
        curSourceLocation->endColumn = (int32_t)area->end_column;
        curSourceLocation->startLine = (int32_t)area->start_line;
        curSourceLocation->endLine = (int32_t)area->end_line;
        curSourceLocation->nextAddress = (uint64_t)area->next_address;
        curSourceLocation->nextStmtAddress = (uint64_t)area->next_stmt_address;
    }

    /**
    * @brief
    Asynchronous function that initiates the address-to-line mapping process for a given range of addresses.
    * @details
    This function retrieves the current TCF context, calls a function to map addresses
    to line numbers within the specified range,
    and signals a condition variable once the mapping is complete.
    * @param arg A void pointer to a user-defined structure that holds the 
    start and end addresses, buffer, and synchronization primitives. 
    **/
    void addr_to_line_async(void* arg)
    {
        auto locationsWrapper = static_cast<stapinlogic::Source_loc_tracker*>(arg);
        auto tcfContext = stapincontext::get_TCF_context(locationsWrapper->pid, locationsWrapper->tid );
        if(nullptr != tcfContext)
        {
            address_to_line(tcfContext, locationsWrapper->startaddress, locationsWrapper->endAddress, &line_numbers_callback, locationsWrapper);
        }
        notify_conditional_variable(locationsWrapper);
    }
} //anonymus namespace

namespace sourcelocationlogic{
    size_t get_source_location(stapinlogic::Source_loc_tracker *locationsWrapper){
        std::unique_lock<std::mutex> lock(locationsWrapper->mtx);
        locationsWrapper->ready = false; 
        stapinlogic::push_func_to_event_loop(addr_to_line_async, locationsWrapper);
        locationsWrapper->cv.wait(lock, [&locationsWrapper] { return locationsWrapper->ready; });
        return locationsWrapper->curIndex;
    }
}
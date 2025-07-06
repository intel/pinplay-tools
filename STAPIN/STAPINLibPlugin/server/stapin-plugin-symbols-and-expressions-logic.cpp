/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#include "stapin-plugin-symbols-and-expressions-logic.h"

namespace symbolsandexpressionslogic{
    void insert_name_or_ID_to_registry_buffer(const std::string& nameOrID, stapinlogic::Registry* registry, bool isName, bool isSymbol) {
        if(nameOrID.empty())
        {   
            return;
        }
        auto requiredSize = nameOrID.length(); 
        if(isName)
        {
            if (registry->curNamesFillingIndex + requiredSize > registry->namesBufferCapacity) 
            {
                if(isSymbol) 
                {
                    std::lock_guard<std::mutex> lock(registry->mtx);
                    (void)registry->name2index.insert(std::make_pair(nameOrID, stapinlogic::NO_OFFSET));
                }
                //TODO write to log
                return;
            }
            memcpy(registry->names.get() + registry->curNamesFillingIndex, nameOrID.c_str(), requiredSize);
            {
                std::lock_guard<std::mutex> lock(registry->mtx);
                (void)registry->name2index.insert(std::make_pair(nameOrID, registry->curNamesFillingIndex));
            }
            registry->curNamesFillingIndex += requiredSize;
            registry->names.get()[registry->curNamesFillingIndex++] = stapinlogic::NULL_TERMINATOR;
        }
        else
        {
            if (registry->curUniqueIDFillingIndex + requiredSize > registry->uniqueIDsBufferCapacity)
            {
                //TODO write to log
                return;
            }
            memcpy(registry->uniqueIDS.get() + registry->curUniqueIDFillingIndex, nameOrID.c_str(), requiredSize);
            {
                std::lock_guard<std::mutex> lock(registry->mtx);
                (void)registry->uniqueID2index.insert(std::make_pair(nameOrID,  registry->curUniqueIDFillingIndex));
            }
            registry->curUniqueIDFillingIndex += requiredSize; 
            registry->uniqueIDS.get()[registry->curUniqueIDFillingIndex++] = stapinlogic::NULL_TERMINATOR;
        }
    }
};


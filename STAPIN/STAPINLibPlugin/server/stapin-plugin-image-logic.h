/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifndef IMAGE_LOGIC_H
#define IMAGE_LOGIC_H

#include "server-utils.h"

namespace imagelogic
{
    /**
    * @brief 
    Inserts a set of memory sections into a global map and updates a global set.
    Each section is represented by a MemoryRegion structure.
    Parameters:
    - @param imageName: A pointer to a character array containing the name of the image.
    - @param sectionNames: A pointer to a character array containing the concatenated names of the sections,
                    separated by null terminators.
    - @param RpcSections: A pointer to an array of stapin::Rpc_section structures containing the section data.
    - @param numSections: The number of sections to be inserted.
    * @return
    - true if the sections are successfully inserted.
    - false if arguments are not valid.
    */
    bool insert_sections_into_list(const char* imageName,  const char* sectionNames, 
                                stapin::Rpc_section* RpcSections , size_t numSections , size_t sectionNamesBufferSize);

    /** 
    * @brief Removes the set of memory sections associated with a given image name from the global map.
    Parameters:
    - @param imageName: A pointer to a character array containing the name of the image whose sections are to be removed.
    * @return
    - true if the sections are successfully removed.
    - false if the image name is not found in the map or got nullptr.
    */
    bool delete_image_sections_from_sections_list(const char* imageName);
};


#endif
/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#include "stapin-plugin-image-logic.h"

#include <string>

namespace{
    stapinlogic::t_memory_region_list& sections_list() noexcept
    {
        //static field in funciton always initialized when access to function
        static stapinlogic::t_memory_region_list sectionsList;
        return sectionsList;
    }
}//anonymus namesace

namespace imagelogic
{
    bool insert_sections_into_list(const char* imageName,  const char* sectionNames, stapin::Rpc_section* RpcSections , size_t numSections, size_t sectionNamesBufferSize){
        if (0 == numSections || nullptr == imageName  || nullptr == sectionNames  || nullptr == RpcSections ) 
        {
            //TODO write to log
            return false;
        }
        stapinlogic::t_memory_region_list sections; 
        std::shared_ptr<std::string> sharedImageName(new (std::nothrow) std::string(imageName));
        if(nullptr == sharedImageName)
        {
            //TODO write to log
            return false;
        }
        for (size_t i = 0; i < numSections; ++i) 
        {
            auto& rpcSection = RpcSections[i];
            if(sectionNamesBufferSize <  rpcSection.sectionNameOffset)
            {
                //TODO write to log
                return false; 
            }
            const char* sectionNamePtr = sectionNames + rpcSection.sectionNameOffset;
            stapinlogic::STAPIN_memory_region section; 
            section.fileName = sharedImageName;
            section.addr = rpcSection.sectionAddress;
            section.sectionName = sectionNamePtr;
            section.flags = rpcSection.mmFlags;
            section.size = rpcSection.sectionSize;
            section.bss = rpcSection.isBSS;
            section.fileOffs = rpcSection.fileOffset;
            section.fileSize = 0;
            section.valid = 1;
            section.ino = 0;
            section.dev = 0;
            sections.push_back(section);
        }
        //No need to lock because Pin serializes instrumentation callbacks like image load and unload.
        sections_list().merge(std::move(sections), stapinlogic::Memory_region_comparator{});
        return true;
    }

    bool delete_image_sections_from_sections_list(const char* imageName)
    {
        if(nullptr == imageName)
        {
            //TODO write to log
            return false;
        }
        //No need to lock because Pin serializes instrumentation callbacks like image load and unload.
        // go over list and compare filename of each section to 
        for (auto it = sections_list().begin(); it != sections_list().end();)
        {
            if(0 == it->fileName.get()->compare(imageName))
            {
                it = sections_list().erase(it);
            }
            else
            {
                ++it;
            }
        }
        return true;
    }

    stapinlogic::t_memory_region_list& get_sections_from_plugin()
    { 
        return sections_list();
    }
};

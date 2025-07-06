/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifndef LNT_H
#define LNT_H

#include <set>
namespace LNT{
  class LINEINFO
  {
  public:
    LINEINFO()
    {
      //std::cerr << "Constructing LINEINFO" << std::endl;
      lineno = 0; 
      low_pc = 0;
      high_pc = 0;
      static_interference_count = 0;
      execution_count =0;
    }
  VOID PrintLineInfo(std::ofstream *outstream)
  {
    //*outstream << "FileName, LineNo, LowPC, HighPC" << std::endl;
    *outstream << filename << "," << std::dec << lineno << ", 0x" << std::hex <<  low_pc << ", 0x" << std::hex << high_pc  << std::endl;
  }
  VOID FiniHeader(std::ofstream *outstream)
  {
    *outstream << "FileName, LineNo, LowPC, HighPC, InterferenceCount, ExecutionCount" << std::endl;
  }
  VOID Fini(std::ofstream *outstream)
  {
    *outstream << filename << "," << std::dec << lineno << ", 0x" << std::hex <<  low_pc << ", 0x" << std::hex << high_pc  << ", " << std::dec << static_interference_count << ", " << execution_count << std::endl;
  }
    UINT32 lineno; 
    std::string filename;
    ADDRINT low_pc;
    ADDRINT high_pc;
    UINT64  static_interference_count; // Other lines sharing instructions with this line
    UINT64  execution_count; // for this line or its low_pc?
    std::set<ADDRINT> ins_addrs;
  };
}
#endif

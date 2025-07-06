/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifndef SLT_H
#define SLT_H

#include <unordered_map>
using std::string;
namespace SLT{
  enum LOCATION {INVALID, MEMORY, REGISTER, OPTIMIZED_AWAY,  UNKNOWN, FETCH_ERROR, DWARF_STACK_VAL, BIT_FIELD, COUNT};
  const char * LocName[] = {"Invalid", "Memory", "Register", "Optimized_away", "Unknown", "FETCH_ERROR",  "Dwarf_stack_value", "Bit_field", "Illegal"};
  class VARLOCINFO
  {
  public:
    VARLOCINFO()
    {
      std::cerr << "Constructing VARLOCINFO" << std::endl;
      for (auto i=0; i<LOCATION::COUNT; i++) location_count[i]=0;
    }
  VOID Fini(std::ofstream *outstream)
  {
    *outstream << "Variable"; 
    for (auto i=0; i<LOCATION::COUNT; i++) 
    {
      *outstream << "," << LocName[i];
    }
    *outstream << std::endl << name; 
    //for (auto i=0; i<LOCATION::COUNT; i++) 
    //{
      //*outstream << "," << std::dec << static_location_count[i];
    //}
    for (auto i=0; i<LOCATION::COUNT; i++) 
    {
      *outstream << "," << std::dec << location_count[i];
    }
  }
    string name;
    string uniqueId;
    UINT64  location_count[LOCATION::COUNT]; // Indexd by enum LOCATION
    UINT64  static_location_count[LOCATION::COUNT]; // Indexd by enum LOCATION
    std::unordered_map<ADDRINT, LOCATION> loc_map;
  };
}
#endif

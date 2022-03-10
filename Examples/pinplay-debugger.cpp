// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause
/*BEGIN_LEGAL 
BSD License 

Copyright (c)2022 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
#include "pin.H"
#include "pinplay.H"
#include "sde-init.H"
#include "sde-pinplay-supp.H"
#include "instlib.H"

using namespace INSTLIB;
using namespace CONTROLLER;

#define KNOB_GDB_COMMAND_FILENAME "cmd_filename"
#define KNOB_FAMILY "pintool:play-debugger"

static PINPLAY_ENGINE *pinplay_engine;
CONTROL_MANAGER * control_manager = NULL;

KNOB_COMMENT pinplay_driver_knob_family(KNOB_FAMILY, "PinPlay Driver Knobs");

KNOB<string> KnobDebugCommandFilename(KNOB_MODE_WRITEONCE,
                    KNOB_FAMILY,
                    KNOB_GDB_COMMAND_FILENAME,
                    "gdb.cmd",
                    "File where 'target remote' command will be \
                    output so a script can monitor it and invoke debugger.");

static BOOL DebugInterpreter(THREADID, CONTEXT *, const string &, 
    string *, VOID *);
static std::string TrimWhitespace(const std::string &);


int SetDebugMode(const string& filename)
{
      DEBUG_MODE mode;
      mode._type = DEBUG_CONNECTION_TYPE_TCP_SERVER; 
      // Pin listen to a port, wait for the debugger to connect
      
      if(filename=="")
          mode._options = DEBUG_MODE_OPTION_STOP_AT_ENTRY;
      else
      mode._options = DEBUG_MODE_OPTION_STOP_AT_ENTRY 
        | DEBUG_MODE_OPTION_SILENT;
      
      if (!PIN_SetDebugMode(&mode))
      {
      ASSERT(0,"Error from PIN_SetDebugMode().\n");
      }
      return 0;
}


/* if gdbFileName is not empty, then output target remote :portNumber to 
 * file gdbFileName.
 * 
 */
int OutputTargetRemoteInfo(const string &filename)
{
      
      DEBUG_CONNECTION_INFO info;
      
      if (!PIN_GetDebugConnectionInfo(&info) || 
      info._type != DEBUG_CONNECTION_TYPE_TCP_SERVER)
      {
      ASSERT(0,"Error from PIN_GetDebugConnectionInfo().\n");
      }
      
      if(filename.empty())
      {
     /* cout << "target remote :" << std::dec 
          << info._tcpServer._tcpPort << "\n";
          */
      }
      else
      {
    // output target remote :portNumber to gdbFileName file.
      std::ofstream Output(filename.c_str());
      ASSERT(Output.is_open(),
        "Could not open"+filename+"\n");
      Output << "target remote :" << std::dec 
          << info._tcpServer._tcpPort << "\n";
      Output << std::flush;
      Output.close();
      }
      
      return 0;
}



static VOID CheckKnobConfiguration()
{    
    string filename = KnobDebugCommandFilename.Value(); 
    if ( !pinplay_engine->IsReplayerActive() && !pinplay_engine->IsLoggerActive() )
    {
        cerr << string("Tool must specify \"-log\", \"-replay\" or both.\n").c_str();
        exit(1);
    }
    //only logging,
    //do not need to specify -log:pintool_control for relogging phase
    if ( pinplay_engine->IsLoggerActive() && !pinplay_engine->IsReplayerActive() )
    {
         cerr <<  
         "Tool must specify (-log:)pintool_control if -log is specified.\n";
        exit(1);
    }
    // do not need to specify -appdebug for logging phase to connect GDB
    if ( pinplay_engine->IsLoggerActive() && !pinplay_engine->IsReplayerActive())
    {
          SetDebugMode(filename);
      OutputTargetRemoteInfo(filename);
    }
    else // need to specify -appdebug if want to connect to GDB
   {
        DEBUG_STATUS debugStatus = PIN_GetDebugStatus();
    
    if(debugStatus != DEBUG_STATUS_DISABLED)
    {
        SetDebugMode(filename);
        OutputTargetRemoteInfo(filename);
    }
   }
}

/*
 * This call-back implements the extended debugger commands.
 *
 *  ctxt[in,out]    Register state for the debugger's "focus" thread.
 *  cmd[in]         Text of the extended command.
 *  result[out]     Text that the debugger prints when the command finishes.
 *
 * Returns: TRUE if we recognize this extended command.
 */
static BOOL DebugInterpreter(THREADID tid, CONTEXT *ctxt, 
    const string &cmd, string *result, VOID *v)
{
    static UINT32 region_no=0;
    std::string line = TrimWhitespace(cmd);
    *result = "";
    static BOOL in_region = FALSE;

    if (line == "help")
    {
        result->append("record on        -- Start logging.\n");
        result->append("record off       -- Stop logging.\n");
        return TRUE;
    }
    else if (line == "record on")
    {
        if(in_region)
        {
            cerr << "Already recording a region. Ignoring.\n";
            return TRUE;
        }
        //control_manager->PintoolControl(CONTROL_START, ctxt, 
        control_manager->Fire(EVENT_START, ctxt, 
            (VOID *)PIN_GetContextReg(ctxt, REG_INST_PTR), tid, FALSE, NULL); 
        result->append("Started recording region number "
            +decstr(region_no)+"\n");
        in_region = TRUE;
        return TRUE;
    }
    else if (line == "record off")
    {
        if(!in_region)
        {
            cerr << "Not recording a region. Ignoring.\n";
            return TRUE;
        }
        //control_manager->PintoolControl(CONTROL_STOP, ctxt, 
        control_manager->Fire(EVENT_STOP, ctxt, 
            (VOID *)PIN_GetContextReg(ctxt, REG_INST_PTR), tid, FALSE, NULL); 
        result->append("Stopped recording region number "
            +decstr(region_no)+"\n");
        region_no++;
        in_region = FALSE;
        return TRUE;
    }

    return FALSE;   /* Unknown command */
}

/*
 * Trim whitespace from a line of text.  Leading and trailing whitespace 
 *  is removed.
 * Any internal whitespace is replaced with a single space (' ') character.
 *
 *  inLine[in]  Input text line.
 *
 * Returns: A string with the whitespace trimmed.
 */
static std::string TrimWhitespace(const std::string &inLine)
{
    std::string outLine = inLine;

    bool skipNextSpace = true;
    for (std::string::iterator it = outLine.begin();  it != outLine.end();  
        ++it)
    {
        if (std::isspace(*it))
        {
            if (skipNextSpace)
            {
                it = outLine.erase(it);
                if (it == outLine.end())
                    break;
            }
            else
            {
                *it = ' ';
                skipNextSpace = true;
            }
        }
        else
        {
            skipNextSpace = false;
        }
    }
    if (!outLine.empty())
    {
        std::string::reverse_iterator it = outLine.rbegin();
        if (std::isspace(*it))
            outLine.erase(outLine.size()-1);
    }
    return outLine;
}

int main(int argc, char *argv[])
{
    sde_pin_init(argc,argv);

    pinplay_engine = sde_tracing_get_pinplay_engine();
    //control_manager = SDE_CONTROLLER::sde_controller_get();
    control_manager = pinplay_engine->LoggerGetController();

    CheckKnobConfiguration();

    PIN_AddDebugInterpreter(DebugInterpreter, 0);

    sde_init();    

    PIN_StartProgram();
}

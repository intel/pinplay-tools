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
#include "isimpoint_inst.H"
#include "instlib.H"
#include "pinplay-debugger-shell.H"
#  include "sde-init.H"
#  include "sde-control.H"
#include "sde-pinplay-supp.H"

using namespace INSTLIB;

PINPLAY_ENGINE *my_pinplay_engine;
ISIMPOINT isimpoint;
DR_DEBUGGER_SHELL::ICUSTOM_INSTRUMENTOR *
    CreatePinPlayInstrumentor(DR_DEBUGGER_SHELL::ISHELL *);
DR_DEBUGGER_SHELL::ISHELL *shell = NULL; 

DR_DEBUGGER_SHELL::ICUSTOM_INSTRUMENTOR 
    *CreatePinPlayInstrumentor(DR_DEBUGGER_SHELL::ISHELL *shell)
{
    return new PINPLAY_DEBUGGER_INSTRUMENTOR(shell);
}

int 
main(int argc, char *argv[])
{
     sde_pin_init(argc,argv);
     sde_init();
     PIN_InitSymbols();
     my_pinplay_engine = sde_tracing_get_pinplay_engine();
    DEBUG_STATUS debugStatus = PIN_GetDebugStatus();

    if (debugStatus != DEBUG_STATUS_DISABLED) 
    {
        shell = DR_DEBUGGER_SHELL::CreatePinPlayShell();
        DR_DEBUGGER_SHELL::STARTUP_ARGUMENTS args;
        if (my_pinplay_engine->IsReplayerActive()) 
        {
            args._customInstrumentor = CreatePinPlayInstrumentor(shell);
        }
        if (!shell->Enable(args))
            return 1;
    }

    isimpoint.activate(argc, argv);

    PIN_StartProgram();
}

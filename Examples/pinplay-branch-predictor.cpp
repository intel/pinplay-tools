/*BEGIN_LEGAL 
BSD License 

Copyright (c)2022 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
#include <iostream>

#include "pin.H"
#include "instlib.H"
#  include "sde-init.H"


using namespace std; 

#include "bimodal.H"
#include "pinplay.H"
#include "sde-pinplay-supp.H"

static BIMODAL bimodal;

using namespace INSTLIB; 

static ofstream *outfile;

#define KNOB_FAMILY "pintool:pinplay-driver"


static PINPLAY_ENGINE *pinplay_engine;

KNOB_COMMENT pinplay_driver_knob_family(KNOB_FAMILY, "PinPlay Driver Knobs");

KNOB<UINT64> KnobPhases(KNOB_MODE_WRITEONCE, 
                        "pintool",
                        "phaselen",
                        "100000000", 
                        "Print branch mispredict stats every these many instructions (and also at the end).\n");
KNOB<string>KnobStatFileName(KNOB_MODE_WRITEONCE,  "pintool",
                     "statfile", "bimodal.out", "Name of the branch predictor stats file.");


static void threadCreated(THREADID threadIndex, CONTEXT *, INT32 , VOID *)
{
    if (threadIndex > 0)
    {
        cerr << "More than one thread detected. This tool currently works only for single-threaded programs/pinballs." << endl;
        exit(0);
    }
}


int main(int argc, char *argv[])
{
    sde_pin_init(argc,argv);
    sde_init();
    PIN_InitSymbols();

    outfile = new ofstream(KnobStatFileName.Value().c_str());
    bimodal.Activate(KnobPhases, outfile);

    pinplay_engine = sde_tracing_get_pinplay_engine();
    
    PIN_AddThreadStartFunction(threadCreated, reinterpret_cast<void *>(0));


    PIN_StartProgram();
}

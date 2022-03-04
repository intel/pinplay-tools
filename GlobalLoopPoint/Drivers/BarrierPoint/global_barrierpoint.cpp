/*BEGIN_LEGAL 
BSD License 

Copyright (c)2022 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/*
  This file creates a PinPlay driver with the capability to gather BBVs
  using barriers.
*/

#include "pinplay.H"
#include "global_isimpoint_inst.H"
#include "global_barrierpoint.H"
#include "control_manager.H"
#include "global_pcregions_control.H"
#include "global_iregions_control.H"
#  include "sde-init.H"
#  include "sde-control.H"

#if defined(PINPLAY)
static GLOBALISIMPOINT pp_gisimpoint;
static PINPLAY_ENGINE pp_pinplay_engine;
#include "sde-pinplay-supp.H"
static PINPLAY_ENGINE *pinplay_engine;
#endif

using namespace dcfg_pin_api;
using namespace CONTROLLER;

static GLOBALISIMPOINT *gisimpoint;
barrierpoint::BARRIERPOINT barrierPoint;
CONTROL_MANAGER * control_manager = NULL;
FILTER_MOD filter;


CONTROL_ARGS args("","pintool:control:pinplay");

int main(int argc, char* argv[])
{
  CONTROL_GLOBALPCREGIONS *pcregions = new CONTROL_GLOBALPCREGIONS(args);
  CONTROL_GLOBALIREGIONS *iregions = new CONTROL_GLOBALIREGIONS(args);
  filter.Activate();
    sde_pin_init(argc,argv);
    sde_init();

    PIN_InitSymbols();
    // This is a replay-only tool (for now)
    pinplay_engine = sde_tracing_get_pinplay_engine();

    control_manager = SDE_CONTROLLER::sde_controller_get();
   gisimpoint = &pp_gisimpoint;
   gisimpoint->activate(argc, argv, &filter);

    // Activate DCFG generation if enabling knob was used.
    DCFG_PIN_MANAGER* dcfgMgr = DCFG_PIN_MANAGER::new_manager();
    if (dcfgMgr->dcfg_enable_knob()) {
        dcfgMgr->activate(pinplay_engine);
    }

    // Activate loop profiling.
    barrierPoint.activate(gisimpoint);

    pcregions->Activate(control_manager);
    iregions->Activate(control_manager);
    PIN_StartProgram();    // Never returns
 
    return 0;
}

/*========================== begin_copyright_notice ============================
# Copyright (C) 2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
============================= end_copyright_notice ===========================*/

/*!
 * @file A GTPin tool that adds no extra instructions but activates all GTPin flows
 */

#include <fstream>
#include <set>

#include "gtpin_api.h"
#include "gtpin_tool_utils.h"
#include "GTPinShim.h"

using namespace gtpin;
using namespace std;

cpu_on_kernel_build_ptr_t cpu_on_kernel_build = 0;
cpu_on_kernel_run_ptr_t cpu_on_kernel_run = 0;
cpu_on_kernel_complete_ptr_t cpu_on_kernel_complete = 0;
cpu_on_gpu_fini_ptr_t cpu_on_gpu_fini = 0;


/* ============================================================================================= */
// Configuration
/* ============================================================================================= */
Knob<int> knobExpectedEnqueueCount("num_enqueues", 0, "Number of enqueues in the application."
                                                      " 0 - unknow number of enqueues");

Knob<bool> knobNoOutput("no_output", false, "Do not store profile data in file");

/* ============================================================================================= */
// Class GTPinShimTool
/* ============================================================================================= */
/*!
 * A GTPin tool that adds no extra instructions but activates all GTPin flows.
 * The tool verifies that the amounts of OnKernelRun and OnKernelComplete events are equal.
 * Additionally, the tool counts the number of Enqueue/Draws in the application.
 * Allows a CPU-Pin tool to register callbacks on kernel run and on kernel complete.
 */
class GTPinShimTool : public GtTool
{
public:
    /// Implementation of the IGtTool interface
    const char* Name() const { return "GTPinShimTool"; }

    void OnKernelBuild(IGtKernelInstrument& instrumentor)
    {
       const IGtKernel& kernel = instrumentor.Kernel();
        if(cpu_on_kernel_build)
        {
           (*cpu_on_kernel_build)(kernel.Name().Get());
        }
    }

    void OnKernelRun(IGtKernelDispatch& dispatcher)
    {
        const IGtKernel& kernel   = dispatcher.Kernel();
        GtGpuPlatform    platform = kernel.GpuPlatform();
        GtKernelExecDesc execDesc; dispatcher.GetExecDescriptor(execDesc);
        bool isProfileEnabled = IsKernelExecProfileEnabled(execDesc, platform);

        dispatcher.SetProfilingMode(isProfileEnabled);
        _enqueues.emplace(execDesc.ToString(platform));
        if(cpu_on_kernel_run) 
        {
          (*cpu_on_kernel_run)(kernel.Name().Get());
        }
        ++_runCounter;
    }

    void OnKernelComplete(IGtKernelDispatch& dispatcher)
    {
       const IGtKernel& kernel = dispatcher.Kernel();
        if(cpu_on_kernel_complete)
        {
           (*cpu_on_kernel_complete)(kernel.Name().Get());
        }
        ++_completeCounter;
    }

    /// @return Single instance of this class
    static GTPinShimTool* Instance()
    {
        static GTPinShimTool instance;
        return &instance;
    }

    /// Callback function registered with atexit()
    static void OnFini()
    {
        if(cpu_on_gpu_fini)
        {
           (*cpu_on_gpu_fini)();
        }
        if (knobNoOutput)
        {
            return;
        }

        IGtCore* gtpinCore = GTPin_GetCore();
        GTPinShimTool& me = *Instance();
        uint32_t expectedEnqueueCount = knobExpectedEnqueueCount;

        gtpinCore->CreateProfileDir();
        std::ofstream fs(JoinPath(gtpinCore->ProfileDir(), "report.txt"));
        //fs  << " GTPin_Entry called. CPU_var " << CPU_var << endl;
        fs << "OnKernelRun calls:      " << me._runCounter << std::endl;
        fs << "OnKernelComplete calls: " << me._completeCounter << std::endl;
        fs << "Enqueues/Draws count: "   << me._enqueues.size() << std::endl;
        if (expectedEnqueueCount != 0)
        {
            fs << "Expected Enqueues/Draws count: "  << expectedEnqueueCount << std::endl;
        }

        bool success = true;
        if (me._completeCounter != me._runCounter)
        {
            fs << "Number of OnKernelComplete callbacks mismatched the number of OnKernelRun callbacks" << std::endl;
            success = false;
        }

        if ((expectedEnqueueCount != 0) && (expectedEnqueueCount != me._enqueues.size()))
        {
            fs << "Number of Enqueues/Draws mismatched the expected number" << std::endl;
            success = false;
        }

        fs << (success ? "PASSED" : "FAILED") << std::endl;
    }

private:
    uint64_t                _runCounter = 0;
    uint64_t                _completeCounter = 0;
    std::set<std::string>   _enqueues;
};

/* ============================================================================================= */
// GTPin_Entry
/* ============================================================================================= */
EXPORT_C_FUNC void GTPin_Entry(int argc, const char *argv[])
{
    SetKnobValue<bool>(true, "always_allocate_buffers"); // Enforce profile buffer allocation to check the buffer-dependent flows
    SetKnobValue<bool>(true, "no_empty_profile_dir");    // Do not create empty profile directory
    ConfigureGTPin(argc, argv);

    GTPinShimTool::Instance()->Register();
    atexit(GTPinShimTool::OnFini);
}

EXPORT_C_FUNC void GTPinShimRegisterCallbacks(void * ptrb, void * ptrr, void * ptrc, void * ptrf) 
{
    cpu_on_kernel_build = (cpu_on_kernel_build_ptr_t) ptrb;
    cpu_on_kernel_run = (cpu_on_kernel_run_ptr_t) ptrr;
    cpu_on_kernel_complete = (cpu_on_kernel_complete_ptr_t) ptrc;
    cpu_on_gpu_fini = (cpu_on_gpu_fini_ptr_t) ptrf;
}

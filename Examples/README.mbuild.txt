You need the following installations:
-------------------------------------
Linux:
Python 3.7
One of GCC versions : 8.3 9.1 10.1
GNU Binutils 2.29
mubild:python-based build system from https://github.com/intelxed/mbuild


You need to set two environment variables for this to work:
-----------------------------------------------------------
1) SDE_BUILD_KIT pointing to the root of the build kit.

2) PYTHONPATH pointing to the directory containing build_kit.py and
the directory containing the mbuild/mbuild directory. 

Run mfile.py using python 3.7 or later.

For example, using a tcsh/csh like shell on non-windows:

% setenv SDE_BUILD_KIT  <Path to SDE kit>

% setenv PYTHONPATH $SDE_BUILD_KIT/pinplay-scripts:<path to mbuild>

% ./mfile.py --host-cpu ia32
% ./mfile.py --host-cpu x86-64

(Use "--help" to see more options on the mfile execution)

Then once your tool is built for 32b and 64b you can copy it to the
i32 and intel64 directories of the kit.

% cp obj-ia32/agen-example.so $SDE_BUILD_KIT/ia32
% cp obj-intel64/agen-example.so $SDE_BUILD_KIT/intel64

% $SDE_BUILD_KIT/sde -t agen-example.so -- your application [args]

To build your own pintool:

1) Copy your tool to the sample diretory
2) edit mfile.py 
   a) Modify tool_sources array to include the new source
   b) Edit tools array to include new tool

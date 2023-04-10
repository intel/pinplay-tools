#!/bin/bash
set -x
rm -rf obj-*
make TARGET=intel64
#make obj-intel64/pinplay-debugger.so
cp obj-intel64/pinplay-debugger.so $SDE_BUILD_KIT/intel64/sde-pinplay-debugger.so
make TARGET=ia32
#make obj-ia32/pinplay-debugger.so
cp obj-ia32/pinplay-debugger.so $SDE_BUILD_KIT/ia32/sde-pinplay-debugger.so

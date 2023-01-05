#!/bin/bash
set -x
echo "Building dlopen-driver.so Pin tool"
make clean; make
cd MyLib
make clean; make
#$PIN_ROOT/pin -t obj-intel64/dlopen-driver.so -probemode -mydll MyLib/libmydll.so -- /bin/ls

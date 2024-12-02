#!/bin/bash
set -x

if [[  -z $SDE_BUILD_KIT ]]; then
    echo "SDE_BUILD_KIT not defined"
    exit 1
fi

if [ ! -e $SDE_BUILD_KIT/pinplay-scripts ];
then
  echo "$SDE_BUILD_KIT/pinplay-scripts does not exist"
  cp -r ../pinplay-scripts $SDE_BUILD_KIT
fi


cd ../Examples
rm -rf obj-*
make TARGET=intel64
#make obj-intel64/pinplay-debugger.so
cp obj-intel64/pinplay-debugger.so $SDE_BUILD_KIT/intel64/sde-pinplay-debugger.so
make TARGET=ia32
#make obj-ia32/pinplay-debugger.so
cp obj-ia32/pinplay-debugger.so $SDE_BUILD_KIT/ia32/sde-pinplay-debugger.so

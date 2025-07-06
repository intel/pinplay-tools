#!/bin/bash
TOOLLOC=$HOME/Tools/STAPIN-Tools/Tools
JSONPATH=$TOOLLOC/ProbeMode/ProbeReuseContext/ReuseContextTool.json
TOOLPATH=$TOOLLOC/ProbeMode/ProbeReuseContext/build/libReuseContextTool.so
ERROR ( )
{
    echo "#Usage: $0 path-to-binary"
    echo ""
    exit
}

if [ $# -lt 1 ]
then
    ERROR
fi

  COMMAND="$1"
eval time $PIN_ROOT/pin -pause_tool 0 -rpc_plugin $JSONPATH -t $TOOLPATH -- $COMMAND 

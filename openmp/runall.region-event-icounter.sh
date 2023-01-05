#!/bin/bash
ulimit -s unlimited
for scr in run.sde-eventcount.*.sh
do 
 echo Running $scr
 time ./$scr
done

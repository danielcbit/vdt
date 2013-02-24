#!/bin/bash

runs=$1;

for (( step=1;  step<=$runs; step++ ))
do
	echo "At step: " $step " from: "$runs
	NS_GLOBAL_VALUE="RngRun=$step" ./waf --run "vdt-tap-wifi-virtual-machine --traceFile='/home/daniel/Downloads/vdt/vdt/mobility_traces.txt' --mNodes=17 --sNodes=0 --duration=1800 --logFile=main-ns2-mob.log --sXMLTemplate='/home/daniel/Downloads/vdt/vdt/node.xml' --mXMLTemplate='/home/daniel/Downloads/vdt/vdt/node.xml' --srcImage='/home/daniel/Downloads/vdt/vdt/setup/hydra-node.image'"
done

exit 0

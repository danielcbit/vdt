#!/bin/bash

folder=$1
runs=$2;


if ((runs >= 1))
then
	for (( step=1;  step<=$runs; step++ ))
	do
	perl ./genContactLogOrig.pl ${folder}/run${step} ${folder}/run${step}/contacts${step}.txt > ${folder}/run${step}/out.txt
	done
fi
exit 0

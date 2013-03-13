#!/bin/bash

folder=$1
runs=$2;


if ((runs >= 1))
then
	pushd $PWD/ContactsLog
	for (( step=1;  step<=$runs; step++ ))
	do
	./parser.bash ${folder}/run${step}/contacts${step}.txt ${folder}/run${step}/contacts${step}_parsed.txt
	done
	popd
fi
exit 0

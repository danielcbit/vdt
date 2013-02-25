#!/bin/bash

runs=$1;


if ((runs >= 1))
then
	for (( step=1;  step<=$runs; step++ ))
	do
		perl ./genContactLog.pl /home/daniel/Dropbox/experimentos/rodadas/run${step} /home/daniel/Dropbox/experimentos/rodadas/run${step}/contacts${step}.txt > /home/daniel/Dropbox/experimentos/rodadas/run${step}/out.txt
	done
fi
exit 0

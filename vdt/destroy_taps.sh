#!/bin/bash

interfaces=$1

if ((interfaces >= 1))
then
	for (( step=0; step<$interfaces; step++ ))
	do
	sudo ifconfig tap${step} down
	sudo tunctl -d tap${step}
	done
fi
exit 0

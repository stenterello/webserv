#!/bin/bash


ADDR=127.0.0.1
PORT=12356
i=0
OCC=1

while [ $OCC == 1 ];
do
	OCC=`curl --dump-header - $ADDR:$PORT 2>/dev/null | grep HTTP | wc -l &`;
	echo $i;
	((i=i+1));
done
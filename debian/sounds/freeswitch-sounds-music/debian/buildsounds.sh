#!/bin/bash

sounds_location=$1
for rate in 32000 16000 8000
do 
    mkdir -p $sounds_location/$rate
    for f in `find $sounds_location/48000 -name \*.wav`
    do
	echo "generating" $sounds_location/$rate/`basename $f`
	sox $f -r $rate $sounds_location/$rate/`basename $f`
    done
done

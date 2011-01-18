#!/bin/bash

sounds_location=$1
for rate in 32000 16000 8000
do 
    for i in ascii base256 conference currency digits ivr misc phonetic-ascii time voicemail zrtp
    do
	mkdir -p $sounds_location/$i/$rate
	for f in `find $sounds_location/$i/48000 -name \*.wav`
	do
	    echo "generating" $sounds_location/$i/$rate/`basename $f`
	    sox $f -r $rate $sounds_location/$i/$rate/`basename $f`
	done
    done
done

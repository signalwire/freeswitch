#!/bin/sh
# Plays a raw file
# usage:
#   playraw file.raw
#   playraw file.raw -d /dev/dsp1 (e.g. for USB headphones)
play -r 8000 -s -2 $1 $2 $3

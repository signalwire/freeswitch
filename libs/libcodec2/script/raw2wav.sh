#!/bin/sh
# Converts 16 bit signed short 8 kHz raw (headerless) files to wave
sox -r 8000 -s -2 $1 $2

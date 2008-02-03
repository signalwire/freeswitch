#!/bin/bash

## check updated ./export against dist, should be something like:
#Only in ../doc: Makefile.am
#Only in ../doc: Makefile.in
#Only in ../doc: mdoc2man.awk
#Only in ../examples: Makefile.am
#Only in ../examples: Makefile.in
#Only in ../examples: fileman.c
#Only in ../src: Makefile.am
#Only in ../src: Makefile.in

for dir in export/*
do
   diff -aur $dir ../${dir#export/}
done


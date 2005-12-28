#!/bin/sh
pwd=$1
shift
mod=$1
shift

make=`which gmake`

if [ -z $make ] ; then
make=`which make`
fi

if [ -f $mod/Makefile ] ; then
$make -C $mod $@
else 
$make -f $pwd/generic_mod.mk -C $mod $@
fi


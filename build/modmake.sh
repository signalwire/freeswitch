#!/bin/sh
pwd=$1
shift
mod=$1
shift

make=`which gmake`

if [ -z $make ] ; then
    make=`which make`
fi

end=`echo $mod | sed "s/^.*\///g"`
if [ -z $end ] ; then
    end=$mod
fi

if [ -f $mod/Makefile ] ; then
    MODNAME=$end $make -C $mod $@
else 
    MODNAME=$end $make -f $pwd/generic_mod.mk -C $mod $@
fi


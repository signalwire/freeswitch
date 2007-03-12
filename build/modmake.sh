#!/bin/sh

make=$1
shift
pwd=$1
shift
mod=$1
shift


end=`echo $mod | sed "s/^.*\///g"`
if [ -z "$end" ] ; then
    end=$mod
fi

if [ -f $mod/Makefile ] ; then
    MOD_CFLAGS="$MOD_CFLAGS" MODNAME=$end BASE=$pwd $make -C $mod $@
else 
    MOD_CFLAGS="$MOD_CFLAGS" MODNAME=$end BASE=$pwd $make -f $pwd/generic_mod.mk -C $mod $@
fi


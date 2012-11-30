#!/bin/sh
s=(`stty size`)
c=${s[1]}


if [ $c -gt 99 ] ; then
    cat ../cluecon2.tmpl
else
    cat ../cluecon2_small.tmpl
fi

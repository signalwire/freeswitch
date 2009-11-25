#!/bin/sh
srcpath=$(dirname $0 2>/dev/null )  || srcpath="." 
$srcpath/configure "$@" --disable-sqlite --disable-shared --with-pic --disable-octave --disable-external-libs


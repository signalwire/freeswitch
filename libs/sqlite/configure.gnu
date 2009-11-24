#! /bin/sh
srcpath=$(dirname $0 2>/dev/null )  || srcpath="." 
$srcpath/configure "$@" --disable-tcl --enable-threadsafe --disable-shared --with-pic


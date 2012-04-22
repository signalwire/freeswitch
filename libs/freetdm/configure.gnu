#! /bin/sh
srcpath=$(dirname $0 2>/dev/null )  || srcpath="." 
$srcpath/configure "$@" --with-pic

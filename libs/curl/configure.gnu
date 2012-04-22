#! /bin/sh
srcpath=$(dirname $0 2>/dev/null )  || srcpath="." 
$srcpath/configure "$@" --without-libidn --disable-shared --with-pic


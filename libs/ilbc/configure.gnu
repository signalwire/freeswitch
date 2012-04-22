#! /bin/sh
srcpath=$(dirname $0)
$srcpath/configure "$@" --with-pic --disable-shared


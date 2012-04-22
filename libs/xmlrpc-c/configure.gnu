#! /bin/sh
srcpath=$(dirname $0 2>/dev/null )  || srcpath="." 
$srcpath/configure "$@" --disable-cplusplus --disable-wininet-client --disable-libwww-client --disable-shared --with-pic --disable-curl-client


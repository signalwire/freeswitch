#! /bin/sh
srcpath=$(dirname $0 2>/dev/null )  || srcpath="."
$srcpath/configure "$@" --with-apr=../apr --disable-shared --with-pic --with-apr-util=../apr-util --with-sofia-sip=../sofia-sip

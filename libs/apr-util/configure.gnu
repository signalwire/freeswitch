#! /bin/sh
srcpath=$(dirname $0 2>/dev/null )  || srcpath="." 
$srcpath/configure "$@" --with-apr=../apr --disable-shared --with-pic --without-sqlite2 --without-sqlite3 --with-expat=builtin


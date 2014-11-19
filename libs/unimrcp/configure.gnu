#! /bin/sh
srcpath=$(pwd)
$srcpath/configure "$@" --with-apr=$srcpath/../apr --disable-shared --with-pic --with-apr-util=$srcpath/../apr-util --with-sofia-sip=$srcpath/../sofia-sip


#!/bin/sh

echo "*** Testing for heap block overruns..."
efrpctest
if ! test $?; then exit 1; fi
 
echo "*** Testing for heap block underruns..."
EF_PROTECT_BELOW=1 efrpctest
if ! test $?; then exit 1; fi

echo "*** Testing for access to freed heap blocks..."
EF_PROTECT_FREE=1 efrpctest
if ! test $?; then exit 1; fi

echo "*** Testing for single-byte overruns..."
EF_ALIGNMENT=0 efrpctest
if ! test $?; then exit 1; fi

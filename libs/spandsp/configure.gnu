#! /bin/sh
ARGS='$@'
MYARGS=`echo $ARGS | sed 's| -Xc | |'`
./configure "$MYARGS" --disable-shared --with-pic --enable-builtin-tiff

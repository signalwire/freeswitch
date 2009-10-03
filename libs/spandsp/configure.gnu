#! /bin/sh
./configure "`echo $@ | sed 's| -Xc | |'`" --disable-shared --with-pic --enable-builtin-tiff

#! /bin/sh
./configure "`echo $@ | sed 's| -xC | |'`" --disable-shared --with-pic --enable-builtin-tiff

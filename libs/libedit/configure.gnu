#! /bin/sh
./configure "$@" CFLAGS="$CFLAGS -D_XOPEN_SOURCE=600 -D_BSD_SOURCE=1" --disable-shared --with-pic

#! /bin/sh
CFLAGS="$CFLAGS -DSU_DEBUG=0 $DEBUG_CFLAGS" ./configure "$@" --with-pic --with-glib=no --disable-shared


#! /bin/sh
CFLAGS="$CFLAGS -DSU_DEBUG=0" ./configure "$@" --with-pic --with-glib=no --disable-shared


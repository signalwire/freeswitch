#!/bin/sh
mkdir -p /shdisk/synswitch
export config_TARGET_CC="aarch64-linux-gnu-gcc"; \
export config_BUILD_CC="gcc"; \
export config_TARGET_CFLAGS=; \
export config_TARGET_LIBS=; \
export CC_FOR_BUILD="gcc"; \
export CFLAGS_FOR_BUILD=" "; \
export enable_ssoft=no; \
export PKG_CONFIG_PATH="/shdisk/lib/pkgconfig"; \
./configure \
CFLAGS=-I/shdisk/include \
CXXFLAGS=-I/shdisk/include \
LIBS=-L/shdisk/lib \
--host=aarch64-linux-gnu \
--build=i686-pc-linux-gnu \
--prefix=/shdisk/synswitch \
--with-dbdir=/var/lib/synswitch/db \
--with-scriptdir=/share/synswitch/scripts \
--with-openssl=pkg-config \
--enable-core-pgsql-support \
#--enable-pool-debug=all \

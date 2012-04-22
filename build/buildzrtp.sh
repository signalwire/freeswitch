#!/bin/sh
tar zxf libzrtp-0.81.514.tar.gz
cd libzrtp-0.81.514
patch -p1 < ../patches/zrtp_bnlib_pic.diff
cd projects/gnu/
./configure CFLAGS="-fPIC"
make
make install

#!/bin/bash
touch noreg
./bootstrap.sh -j
./configure -C --enable-portable-binary \
            --prefix=/usr --localstatedir=/var --sysconfdir=/etc \
            --with-gnu-ld --with-python --with-erlang --with-openssl \
            --enable-core-odbc-support --enable-zrtp \
            --enable-core-pgsql-support \
            --enable-static-v8 
#CC=clang-3.6 CXX=clang++-3.6 ./configure -C --enable-portable-binary \
#           --prefix=/usr --localstatedir=/var --sysconfdir=/etc \
#           --with-gnu-ld --with-python --with-erlang --with-openssl \
#           --enable-core-odbc-support --enable-zrtp \
#           --enable-core-pgsql-support \
#           --enable-static-v8 --disable-parallel-build-v8 --enable-address-sanitizer
make
#make -j install 

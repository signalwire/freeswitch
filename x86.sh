#!/bin/sh
cp -rf ./uc/* ./
./bootstrap.sh -j
./configure
make
make install

#!/bin/bash
export CFLAGS="-ggdb3 -O0"
export CXXFLAGS="-ggdb3 -O0"
./bootstrap.sh -j
./configure $@


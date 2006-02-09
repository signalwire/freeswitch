#!/bin/sh

./configure $@
$MAKE depend
cd libraries
$MAKE
$MAKE install

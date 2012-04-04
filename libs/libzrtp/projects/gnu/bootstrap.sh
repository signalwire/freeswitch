#!/bin/bash

reconf () {
  aclocal
  libtoolize --copy --automake
  autoconf
  autoheader
  automake --no-force --add-missing --copy
}

(cd ../../third_party/bnlib && reconf)
reconf


#!/bin/bash

## increment LT_VERSION in ../configure.ac

f=../configure.ac

grep LT_VERSION $f

i=$(awk '/LT_VERSION/ {n=gensub(/.*:([0-9]+):.*/,"\\1",1);print n}' $f)
((i++))

ed $f <<EOF
1,\$s/AC_SUBST(LT_VERSION, \[0:[0-9]*:0\])/AC_SUBST(LT_VERSION, \[0:$i:0\])/
w
EOF

grep LT_VERSION $f

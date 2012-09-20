#!/bin/sh

# This script works with Solaris Studio 11
# version of dbx which supports "check -all" and
# "check -access" run modes, at least on Sparc.
# These detect access or alignment violations or
# leftover unfreed memory.           TDH 29Dec06

DBX=/opt/SUNWspro/bin/dbx
OUT=/tmp/dbx.out.$$

echo "Writing $OUT..."

for f in test? test??
do
    echo $f
    ${DBX} $f 1>>${OUT} 2>&1 <<EOF
check -all
run
EOF

egrep 'mar|rua|rui|wui|wua|maw' ${OUT}

done

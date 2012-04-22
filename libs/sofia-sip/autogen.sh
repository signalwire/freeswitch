#!/bin/sh

set -x
AUTOMAKE=${AUTOMAKE:-automake-1.9} ACLOCAL=${ACLOCAL:-aclocal-1.9}
export AUTOMAKE ACLOCAL
${AUTORECONF:-autoreconf} -i
find . \( -name 'run*' -o -name '*.sh' \) -a -type f | xargs chmod +x
chmod +x scripts/*

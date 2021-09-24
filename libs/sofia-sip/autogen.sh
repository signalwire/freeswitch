#!/bin/sh

set -x
AUTOMAKE=${AUTOMAKE:-automake} ACLOCAL=${ACLOCAL:-aclocal}
export AUTOMAKE ACLOCAL
${AUTORECONF:-autoreconf} -i
find . \( -name 'run*' -o -name '*.sh' \) -a -type f | xargs chmod +x
chmod +x scripts/*

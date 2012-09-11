#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

src_repo="$(pwd)"

if [ ! -d .git ]; then
  echo "error: must be run from within the top level of a FreeSWITCH git tree." 1>&2
  exit 1;
fi

(mkdir -p rpmbuild && cd rpmbuild && mkdir -p SOURCES BUILD BUILDROOT i386 x86_64 SOURCES SPECS)

cd $src_repo/rpmbuild/SOURCES

for i in `grep 'Source..\?:' $src_repo/freeswitch.spec|grep -v Source0 |awk '{print $2}'`; do wget $i; done

cat 1>&2 <<EOF
----------------------------------------------------------------------
Got the Extra Source Tarballs We Need....
----------------------------------------------------------------------
EOF

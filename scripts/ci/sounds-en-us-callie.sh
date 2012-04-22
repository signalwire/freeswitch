#!/bin/bash
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

src_repo="$(pwd)"

if [ ! -d .git ]; then
  echo "error: must be run from within the top level of a FreeSWITCH git tree." 1>&2
  exit 1;
fi

if [ -z "$1" ]; then
  echo "usage: ./scripts/ci/rpmbuilder.sh MAJOR.MINOR.MICRO[.REVISION] BUILD_NUMBER" 1>&2
  exit 1;
fi

ver="$1"
major=$(echo "$ver" | cut -d. -f1)
minor=$(echo "$ver" | cut -d. -f2)
micro=$(echo "$ver" | cut -d. -f3)

build="$2"

cd rpmbuild/SOURCES

wget http://files.freeswitch.org/freeswitch-sounds-en-us-callie-48000-$ver.tar.gz

cd ../..

rpmbuild --define "VERSION_NUMBER $ver" \
  --define "BUILD_NUMBER 1" \
  --define "_topdir %(pwd)/rpmbuild" \
  --define "_rpmdir %{_topdir}" \
  --define "_srcrpmdir %{_topdir}" \
  -ba freeswitch-sounds-en-us-callie.spec

mkdir $src_repo/RPMS
mv $src_repo/rpmbuild/*/*.rpm $src_repo/RPMS/.

cat 1>&2 <<EOF
----------------------------------------------------------------------
The Sound RPMs have been rolled
----------------------------------------------------------------------
EOF

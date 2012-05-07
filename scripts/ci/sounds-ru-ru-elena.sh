#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

src_repo="$(pwd)"

if [ ! -d .git ]; then
  echo "error: must be run from within the top level of a FreeSWITCH git tree." 1>&2
  exit 1;
fi

ver="1.0.13"

build="2"

cd rpmbuild/SOURCES

wget http://files.freeswitch.org/freeswitch-sounds-ru-RU-elena-48000-$ver.tar.gz

cd ../..

rpmbuild --define "VERSION_NUMBER $ver" \
  --define "BUILD_NUMBER $build" \
  --define "_topdir %(pwd)/rpmbuild" \
  --define "_rpmdir %{_topdir}" \
  --define "_srcrpmdir %{_topdir}" \
  -ba freeswitch-sounds-ru-RU-elena.spec

mkdir $src_repo/RPMS
mv $src_repo/rpmbuild/*/*.rpm $src_repo/RPMS/.

cat 1>&2 <<EOF
----------------------------------------------------------------------
The Sound RPMs have been rolled
----------------------------------------------------------------------
EOF


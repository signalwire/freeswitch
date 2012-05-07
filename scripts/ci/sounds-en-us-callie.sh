#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

sdir="."
[ -n "${0%/*}" ] && sdir="${0%/*}"
. $sdir/common.sh

check_pwd
check_input_ver_build $@
eval $(parse_version "$1")
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

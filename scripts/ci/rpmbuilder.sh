#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

sdir="."
[ -n "${0%/*}" ] && sdir="${0%/*}"
. $sdir/common.sh

check_pwd
check_input_ver_build $@
eval $(parse_version "$1")

if [ -n "$rev" ]; then
  dst_name="freeswitch-$cmajor.$cminor.$cmicro.$rev"
else
  dst_name="freeswitch-$cmajor.$cminor.$cmicro"
fi
dst_parent="/tmp/"
dst_dir="/tmp/$dst_name"
release="1"
if [ $# -gt 1 ]; then
  release="$2"
fi

(mkdir -p rpmbuild && cd rpmbuild && mkdir -p SOURCES BUILD BUILDROOT i386 x86_64 SPECS)

cd $src_repo
cp -a src_dist/*.spec rpmbuild/SPECS/ || true
cp -a src_dist/* rpmbuild/SOURCES/ || true
cd rpmbuild/SPECS
set_fs_release "$release"
cd ../../

rpmbuild --define "_topdir %(pwd)/rpmbuild" \
  --define "_rpmdir %{_topdir}" \
  --define "_srcrpmdir %{_topdir}" \
  -ba rpmbuild/SPECS/freeswitch.spec

mkdir -p $src_repo/RPMS
mv $src_repo/rpmbuild/*/*.rpm $src_repo/RPMS/.

cat 1>&2 <<EOF
----------------------------------------------------------------------
The v$cver-$build RPMs have been rolled, now we
just need to push them to the YUM Repo
----------------------------------------------------------------------
EOF


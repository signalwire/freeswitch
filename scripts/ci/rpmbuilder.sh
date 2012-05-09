#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

sdir="."
[ -n "${0%/*}" ] && sdir="${0%/*}"
. $sdir/common.sh

check_pwd
check_input_ver_build $@
eval $(parse_version "$1")
build="$2"

dst_name="freeswitch-$cmajor.$cminor.$cmicro"
dst_parent="/tmp/"
dst_dir="/tmp/$dst_name"

(mkdir -p rpmbuild && cd rpmbuild && mkdir -p SOURCES BUILD BUILDROOT i386 x86_64 SPECS)

cd $src_repo
cp -a src_dist/* rpmbuild/SOURCES/ || true

rpmbuild --define "VERSION_NUMBER $cver" \
  --define "BUILD_NUMBER $build" \
  --define "_topdir %(pwd)/rpmbuild" \
  --define "_rpmdir %{_topdir}" \
  --define "_srcrpmdir %{_topdir}" \
  -ba freeswitch.spec

# --define '_rpmfilename %%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm' \
# --define "_sourcedir  %{_topdir}" \
# --define "_builddir %{_topdir}" \


mkdir $src_repo/RPMS
mv $src_repo/rpmbuild/*/*.rpm $src_repo/RPMS/.

cat 1>&2 <<EOF
----------------------------------------------------------------------
The v$cver-$build RPMs have been rolled, now we
just need to push them to the YUM Repo
----------------------------------------------------------------------
EOF


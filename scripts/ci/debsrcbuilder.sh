#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

sdir="."
[ -n "${0%/*}" ] && sdir="${0%/*}"
. $sdir/common.sh

eval $(parse_version "$1")
datestamp="$(date +%Y%m%dT%H%M%SZ)"
nightly="n${datestamp}"
build="b$2"
input_distro=$3
distro=${input_distro:="unstable"}

dst_version="$ver"
dst_name="freeswitch-${dst_version}"
dst_parent="/tmp/"
dst_dir="/tmp/${dst_name}"
dst_full_version="${dst_version}~${nightly}~${build}"
dst_full_name="freeswitch-${dst_full_version}"

mkdir -p $src_repo/debbuild/

tar xjf src_dist/${dst_name}.tar.bz2 -C ${src_repo}/debbuild/
mv ${src_repo}/debbuild/${dst_name} ${src_repo}/debbuild/${dst_full_name}
mv src_dist/${dst_name}.tar.bz2 \
  ${src_repo}/debbuild/freeswitch_${dst_full_version}.orig.tar.bz2

# Build the debian source package first, from the source tar file.
echo "changing directory to ${src_repo}/debbuild/${dst_full_name}"

cd ${src_repo}/debbuild/${dst_full_name}
(cd debian && ./bootstrap.sh)
# dependency: libparse-debcontrol-perl
dch -v "${dst_full_version}-1" \
  -M --force-distribution -D "$distro" \
  "Nightly build at ${datestamp}."
# dependency: fakeroot
dpkg-buildpackage -rfakeroot -S -us -uc

status=$?

if [ $status -gt 0 ]; then
  exit $status
else
  cat 1>&2 <<EOF
----------------------------------------------------------------------
The ${dst_full_name} DEB-SRCs have been rolled, now we
just need to push them to the Debian repo
----------------------------------------------------------------------
EOF
fi


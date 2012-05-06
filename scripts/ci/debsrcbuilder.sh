#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

sdir="."
[ -n "${0%/*}" ] && sdir="${0%/*}"
. $sdir/common.sh

eval $(parse_version "$1")
datestamp="$(date +%Y%m%dT%H%M%SZ)"
nightly="n${datestamp}"
build="b$2"
distro=${3:="unstable"}

fver="${ver}~${nightly}~${build}"
fname="freeswitch-$fver"
orig="freeswitch_$fver.orig"
ddir=$src_repo/debbuild
bdir=$src_repo/debbuild/$fname

mkdir -p $ddir
git clone . $bdir
cd $bdir
set_fs_ver "$ver" "$major" "$minor" "$micro" "$rev"
sleep 2
cd $ddir
tar -c --exclude=.git -vf $orig.tar $fname
echo "Compressing $orig.tar with xz -9e..." >&2
xz -9e $orig.tar

cd $bdir
(cd debian && ./bootstrap.sh)
# dch can't handle comments in control file
(cd debian; \
  mv control control.orig; \
  grep -e '^#' -v control.orig > control)
# dependency: libparse-debcontrol-perl
dch -b -v "${fver}-1" \
  -M --force-distribution -D "$distro" \
  "Nightly build at ${datestamp}."
# dependency: fakeroot
dpkg-buildpackage -S -rfakeroot -uc -us -i\.git -I.git -Zxz -z9 || exit ?

cat 1>&2 <<EOF
----------------------------------------------------------------------
The ${fname} DEB-SRCs have been rolled, now we
just need to push them to the Debian repo
----------------------------------------------------------------------
EOF


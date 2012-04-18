#!/bin/bash
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

src_repo="$(pwd)"

if [ ! -d .git ]; then
  echo "error: must be run from within the top level of a FreeSWITCH git tree." 1>&2
  exit 1;
fi

if [ -z "$1" ]; then
  echo "usage: ./scripts/ci/debbuilder.sh MAJOR.MINOR.MICRO[.REVISION] BUILD_NUMBER" 1>&2
  exit 1;
fi

ver="$1"
major=$(echo "$ver" | cut -d. -f1)
minor=$(echo "$ver" | cut -d. -f2)
micro=$(echo "$ver" | cut -d. -f3)
rev=$(echo "$ver" | cut -d. -f4)

build="$2"

dst_version="$major.$minor.$micro"
dst_name="freeswitch-$dst_version"
dst_parent="/tmp/"
dst_dir="/tmp/$dst_name"
dst_version_full="$dst_version.$rev"
dst_name_full="freeswitch-$dst_name_full"

mkdir -p $src_repo/debbuild/

tar xvjf src_dist/$dst_name.tar.bz2 -C $src_repo/debbuild/
mv $src_repo/debbuild/$dst_name $src_repo/debbuild/$dst_name_full
cp src_dist/$dst_name.tar.bz2 $src_repo/debbuild/freeswitch_${dst_version_full}.orig.tar.bz2

# Build the debian source package first, from the source tar file.
cd $src_repo/debbuild/$dst_name_full

dch -v $dst_version-$build "Nightly Build"

dpkg-buildpackage -rfakeroot -S -us -uc

status=$?

if [ $status -gt 0 ]; then
  exit $status
else
  cat 1>&2 <<EOF
----------------------------------------------------------------------
The v$ver-$build DEB-SRCs have been rolled, now we
just need to push them to the YUM Repo
----------------------------------------------------------------------
EOF
fi


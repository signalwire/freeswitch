#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

src_repo="$(pwd)"

if [ ! -d .git ]; then
  echo "error: must be run from within the top level of a FreeSWITCH git tree." 1>&2
  exit 1;
fi

rpmbuild --define "VERSION_NUMBER $1" \
  --define "BUILD_NUMBER $2" \
  --define "_topdir %(pwd)/rpmbuild" \
  --define "_rpmdir %{_topdir}" \
  --define "_srcrpmdir %{_topdir}" \
  -ba freeswitch-config-rayo.spec

mkdir $src_repo/RPMS
mv $src_repo/rpmbuild/*/*.rpm $src_repo/RPMS/.

cat 1>&2 <<EOF
----------------------------------------------------------------------
The Rayo configuration RPMs have been rolled
----------------------------------------------------------------------
EOF


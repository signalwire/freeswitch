#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

src_repo="$(pwd)"

if [ ! -d .git ]; then
  echo "error: must be run from within the top level of a FreeSWITCH git tree." 1>&2
  exit 1;
fi

ver="R14B"
rel="03"

cd rpmbuild/SOURCES

wget http://www.erlang.org/download/otp_src_R14B03.tar.gz
wget http://www.erlang.org/download/otp_doc_html_R14B03.tar.gz
wget http://www.erlang.org/download/otp_doc_man_R14B03.tar.gz

cp ../../scripts/ci/extras/otp-R14B-00-0001-Do-not-format-man-pages.patch .

cd ../..

cp scripts/ci/extras/erlang.spec .

rpmbuild --define "VERSION_NUMBER $ver" \
  --define "RELEASE_NUMBER $rel" \
  --define "_topdir %(pwd)/rpmbuild" \
  --define "_rpmdir %{_topdir}" \
  --define "_srcrpmdir %{_topdir}" \
  -ba erlang.spec

mkdir $src_repo/RPMS
mv $src_repo/rpmbuild/*/*.rpm $src_repo/RPMS/.

cat 1>&2 <<EOF
----------------------------------------------------------------------
The Erlang RPM has been rolled
----------------------------------------------------------------------
EOF


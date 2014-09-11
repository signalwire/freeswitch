dst_parent="/tmp/"
dst_dir="/tmp/$dst_name"
#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

sdir="."
[ -n "${0%/*}" ] && sdir="${0%/*}"

dst_name="freeswitch-release"

(mkdir -p temp && cd temp && mkdir -p SOURCES BUILD BUILDROOT i386 x86_64 SPECS)

cp -a * temp/SOURCES/ || true

rpmbuild --define "_topdir %(pwd)/temp" \
  --define "_rpmdir %{_topdir}" \
  --define "_srcrpmdir %{_topdir}" \
  -ba freeswitch-release.spec

mkdir ./RPMS
mv ./temp/*/*.rpm ./RPMS/.

cat 1>&2 <<EOF
----------------------------------------------------------------------
The Repo RPM has been rolled, now we
just need to push to the web server
----------------------------------------------------------------------
EOF

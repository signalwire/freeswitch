#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

basedir=$(pwd);

if [ ! -d .git ]; then
  echo "error: must be run from within the top level of a FreeSWITCH git tree." 1>&2
  exit 1;
fi

(mkdir -p rpmbuild && cd rpmbuild && mkdir -p SOURCES BUILD BUILDROOT i386 x86_64 SPECS)

if [ ! -d "$basedir/../freeswitch-sounds" ]; then
        cd $basedir/..
        git clone https://stash.freeswitch.org/scm/fs/freeswitch-sounds.git
else
        cd $basedir/../freeswitch-sounds
        git clean -fdx
        git pull
fi

cd $basedir/../freeswitch-sounds/sounds/trunk
./dist.pl ru/RU/elena

mv freeswitch-sounds-ru-RU-elena-*.tar.* $basedir/rpmbuild/SOURCES

cd $basedir

rpmbuild --define "_topdir %(pwd)/rpmbuild" \
  --define "_rpmdir %{_topdir}" \
  --define "_srcrpmdir %{_topdir}" \
  -ba freeswitch-sounds-ru-RU-elena.spec

mkdir $basedir/RPMS
mv $basedir/rpmbuild/*/*.rpm $basedir/RPMS/.

cat 1>&2 <<EOF
----------------------------------------------------------------------
The Sound RPMs have been rolled
----------------------------------------------------------------------
EOF


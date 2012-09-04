#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

sdir="."
[ -n "${0%/*}" ] && sdir="${0%/*}"
. $sdir/common.sh

check_pwd
check_input_ver_build $@
in_ver="$1"
if [ "$in_ver" = "auto" ]; then
  in_ver="$(cat build/next-release.txt)"
fi
eval $(parse_version "$in_ver")
datestamp="$(date +%Y%m%dT%H%M%SZ)"
nightly="n${datestamp}"
build="b${2-0}"
distro="${3-unstable}"
codename="${4-sid}"

fver="${dver}~${nightly}~${build}"
fname="freeswitch-$fver"
orig="freeswitch_$fver.orig"
ddir=$src_repo/debbuild
bdir=$src_repo/debbuild/$fname

mkdir -p $ddir
git clone . $bdir
cd $bdir
set_fs_ver "$gver" "$gmajor" "$gminor" "$gmicro" "$grev"
echo "$gver" > .version
cd libs
getlib () {
  f="${1##*/}"
  echo "fetching: $1 to $f" >&2
  wget -N "$1" \
    && tar -xv --no-same-owner --no-same-permissions -f "$f" \
    && rm -f "$f" \
    && mkdir -p $f
}
getlib http://downloads.mongodb.org/cxx-driver/mongodb-linux-x86_64-v1.8-latest.tgz
getlib http://files.freeswitch.org/downloads/libs/json-c-0.9.tar.gz
getlib http://files.freeswitch.org/downloads/libs/libmemcached-0.32.tar.gz
getlib http://files.freeswitch.org/downloads/libs/soundtouch-1.6.0.tar.gz
getlib http://files.freeswitch.org/downloads/libs/flite-1.5.4-current.tar.bz2
getlib http://files.freeswitch.org/downloads/libs/sphinxbase-0.7.tar.gz
getlib http://files.freeswitch.org/downloads/libs/pocketsphinx-0.7.tar.gz
getlib http://files.freeswitch.org/downloads/libs/communicator_semi_6000_20080321.tar.gz
getlib http://files.freeswitch.org/downloads/libs/celt-0.10.0.tar.gz
getlib http://files.freeswitch.org/downloads/libs/opus-0.9.0.tar.gz
getlib http://files.freeswitch.org/downloads/libs/openldap-2.4.19.tar.gz
getlib http://download.zeromq.org/zeromq-2.1.9.tar.gz \
  || getlib http://download.zeromq.org/historic/zeromq-2.1.9.tar.gz
getlib http://files.freeswitch.org/downloads/libs/freeradius-client-1.1.6.tar.gz
getlib http://files.freeswitch.org/downloads/libs/lame-3.98.4.tar.gz
getlib http://files.freeswitch.org/downloads/libs/libshout-2.2.2.tar.gz
getlib http://files.freeswitch.org/downloads/libs/mpg123-1.13.2.tar.gz
cd mongo-cxx-driver-v1.8
rm -rf config.log .sconf_temp *Test *Example
find . -name "*.o" -exec rm -f {} \;
cd $ddir
tar -c --exclude=.git -vf $orig.tar $fname
echo "Compressing $orig.tar with xz -6..." >&2
xz -6 $orig.tar

cd $bdir
(cd debian && ./bootstrap.sh -c "$codename")
# dch can't handle comments in control file
(cd debian; \
  mv control control.orig; \
  grep -e '^#' -v control.orig > control)
# dependency: libparse-debcontrol-perl
dch -b -v "${fver}-1~${codename}+1" \
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


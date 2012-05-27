#!/bin/bash
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-
##### Author: Travis Cross <tc@traviscross.com>

set -e

ddir="."
[ -n "${0%/*}" ] && ddir="${0%/*}"

err () {
  echo "$0 error: $1" >&2
  exit 1
}

announce () {
  cat >&2 <<EOF

########################################################################
## $1
########################################################################

EOF
}

xread () {
  local xIFS="$IFS"
  IFS=''
  read $@
  local ret=$?
  IFS="$xIFS"
  return $ret
}

create_dbg_pkgs () {
  for x in $ddir/*; do
    test ! -d $x && continue
    test "$x" = "tmp" -o "$x" = "source" && continue
    test ! "$x" = "${x%-dbg}" && continue
    test ! -d $x/usr/lib/debug && continue
    mkdir -p $x-dbg/usr/lib
    mv $x/usr/lib/debug $x-dbg/usr/lib/
  done
}

cwget () {
  local url="$1" f="${1##*/}"
  echo "fetching: $url to $f" >&2
  if [ -n "$FS_FILES_DIR" ]; then
    if ! [ -s "$FS_FILES_DIR/$f" ]; then
      (cd $FS_FILES_DIR && wget -N "$url")
    fi
    cp -a $FS_FILES_DIR/$f .
  else
    wget -N "$url"
  fi
}

getlib () {
  local sd="$1" url="$2" f="${2##*/}"
  (cd $sd/libs \
    && cwget "$url" \
    && tar -xv --no-same-owner --no-same-permissions -f "$f" \
    && rm -f "$f" \
    && mkdir -p $f)
}

getlibs () {
  local sd="$1"
  # get pinned libraries
  getlib $sd http://downloads.mongodb.org/cxx-driver/mongodb-linux-x86_64-v1.8-latest.tgz
  getlib $sd http://files.freeswitch.org/downloads/libs/json-c-0.9.tar.gz
  getlib $sd http://files.freeswitch.org/downloads/libs/libmemcached-0.32.tar.gz
  getlib $sd http://files.freeswitch.org/downloads/libs/soundtouch-1.6.0.tar.gz
  getlib $sd http://files.freeswitch.org/downloads/libs/flite-1.5.4-current.tar.bz2
  getlib $sd http://files.freeswitch.org/downloads/libs/sphinxbase-0.7.tar.gz
  getlib $sd http://files.freeswitch.org/downloads/libs/pocketsphinx-0.7.tar.gz
  getlib $sd http://files.freeswitch.org/downloads/libs/communicator_semi_6000_20080321.tar.gz
  getlib $sd http://files.freeswitch.org/downloads/libs/celt-0.10.0.tar.gz
  getlib $sd http://files.freeswitch.org/downloads/libs/opus-0.9.0.tar.gz
  getlib $sd http://files.freeswitch.org/downloads/libs/openldap-2.4.19.tar.gz
  getlib $sd http://download.zeromq.org/zeromq-2.1.9.tar.gz \
    || getlib $sd http://download.zeromq.org/historic/zeromq-2.1.9.tar.gz
  getlib $sd http://files.freeswitch.org/downloads/libs/freeradius-client-1.1.6.tar.gz
  getlib $sd http://files.freeswitch.org/downloads/libs/lame-3.98.4.tar.gz
  getlib $sd http://files.freeswitch.org/downloads/libs/libshout-2.2.2.tar.gz
  getlib $sd http://files.freeswitch.org/downloads/libs/mpg123-1.13.2.tar.gz
  # cleanup mongo
  (
    cd $sd/libs/mongo-cxx-driver-v1.8
    rm -rf config.log .sconf_temp *Test *Example
    find . -name "*.o" -exec rm -f {} \;
  )
}

get_current_version () {
  cat $ddir/changelog \
    | grep -e '^freeswitch ' \
    | awk '{print $2}' \
    | sed -e 's/[()]//g' -e 's/-.*//'
}

_create_orig () {
  . $ddir/../scripts/ci/common.sh
  eval $(parse_version "$(get_current_version)")
  local destdir="$1" xz_level="$2" n=freeswitch
  local d=${n}-${dver} f=${n}_${dver}
  local sd=${ddir}/sdeb/$d
  [ -n "$destdir" ] || destdir=$ddir/../../
  mkdir -p $sd
  tar -c -C $ddir/../ \
    --exclude=.git \
    --exclude=debian \
    --exclude=freeswitch.xcodeproj \
    --exclude=fscomm \
    --exclude=htdocs \
    --exclude=w32 \
    --exclude=web \
    -vf - . | tar -x -C $sd -vf -
  (cd $sd && set_fs_ver "$gver" "$gmajor" "$gminor" "$gmicro" "$grev")
  getlibs $sd
  tar -c -C $ddir/sdeb -vf $ddir/sdeb/$f.orig.tar $d
  xz -${xz_level}v $ddir/sdeb/$f.orig.tar
  mv $ddir/sdeb/$f.orig.tar.xz $destdir
  rm -rf $ddir/sdeb
}

create_orig () {
  local xz_level="6"
  while getopts 'dz:' o; do
    case "$o" in
      d) set -vx;;
      z) xz_level="$OPTARG";;
    esac
  done
  shift $(($OPTIND-1))
  _create_orig "$1" "$xz_level"
}

create_dsc () {
  . $ddir/../scripts/ci/common.sh
  local xz_level="6"
  while getopts 'dz:' o; do
    case "$o" in
      d) set -vx;;
      z) xz_level="$OPTARG";;
    esac
  done
  shift $(($OPTIND-1))
  eval $(parse_version "$(get_current_version)")
  local destdir="$1" n=freeswitch
  local d=${n}-${dver} f=${n}_${dver}
  [ -n "$destdir" ] || destdir=$ddir/../../
  [ -f $destdir/$f.orig.tar.xz ] \
    || _create_orig "$1" "${xz_level}"
  (
    ddir=$(pwd)/$ddir
    cd $destdir
    mkdir -p $f
    cp -a $ddir $f
    dpkg-source -b -i.* -Zxz -z9 $f
  )
}

build_nightly_for () {
  set -e
  local branch="$1"
  local distro="$2" suite=""
  case $distro in
    experimental) distro="sid" suite="experimental";;
    sid) suite="unstable";;
    wheezy) suite="testing" ;;
    squeeze) suite="stable" ;;
  esac
  [ -x "$(which cowbuilder)" ] \
    || err "Error: package cowbuilder isn't installed"
  [ -x "$(which dch)" ] \
    || err "Error: package devscripts isn't installed"
  [ -x "$(which git-buildpackage)" ] \
    || err "Error: package git-buildpackage isn't installed"
  ulimit -n 200000 || true
  if ! [ -d /var/cache/pbuilder/base-$distro.cow ]; then
    announce "Creating base $distro image..."
    cowbuilder --create \
      --distribution $distro \
      --basepath /var/cache/pbuilder/base-$distro.cow
  fi
  announce "Updating base $distro image..."
  cowbuilder --update \
    --distribution $distro \
    --basepath /var/cache/pbuilder/base-$distro.cow
  local ver="$(cat build/next-release.txt | sed -e 's/-/~/g')~n$(date +%Y%m%dT%H%M%SZ)-1~${distro}+1"
  echo "Building v$ver for $distro based on $branch"
  cd $ddir/../
  announce "Building v$ver..."
  git clean -fdx
  git reset --hard $branch
  ./build/set-fs-version.sh "$ver"
  git add configure.in && git commit --allow-empty -m "nightly v$ver"
  (cd debian && ./bootstrap.sh -c $distro)
  dch -b -m -v "$ver" --force-distribution -D "$suite" "Nightly build."
  git-buildpackage -us -uc \
    --git-verbose \
    --git-pbuilder --git-dist=$distro \
    --git-compression=xz --git-compression-level=9ev
  git reset --hard HEAD^
}

build_nightly () {
  local branch="$1"; shift
  for distro in "$@"; do
    build_nightly_for "$branch" "$distro"
  done
}

cmd="$1"
shift
case "$cmd" in
  build-nightly) build_nightly "$@" ;;
  create-dbg-pkgs) create_dbg_pkgs ;;
  create-dsc) create_dsc "$@" ;;
  create-orig) create_orig "$@" ;;
esac


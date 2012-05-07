#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

src_repo="$(pwd)"
tmp_dir=${TMP_DIR:="/tmp"}

parse_version () {
  local ver="$1" major="" minor="" micro="" rev=""
  local next=major
  for x in $(echo "$1" | sed -e 's/\([._~-]\)/ \1 /g'); do
    if [ $next = rev ]; then
      rev="${rev}${x}"
    elif [ "$x" = "." ] || [ "$x" = "_" ] || [ "$x" = "~" ] || [ "$x" = "-" ]; then
      if [ "$x" = "_" ] || [ "$x" = "~" ] || [ "$x" = "-" ]; then
        next=rev
        eval $next='$x'
      else
        case $next in
          major) next=minor;;
          minor) next=micro;;
          micro) next=rev;;
        esac
      fi
    else
      local tmp="$(eval echo \$$next)"
      eval $next='${tmp}${x}'
    fi
  done
  local cmajor cminor cmicro crev cver
  cmajor=${major:="0"}
  cminor=${minor:="0"}
  cmicro=${micro:="0"}
  crev="$(echo "$rev" | sed -e 's/[._~-]//')"
  cver="${cmajor}.${cminor}.${cmicro}"
  if [ -n "${micro}" ] && echo "$micro" | grep '^\(alpha\|beta\|rc\)' >/dev/null; then
    rev="~${micro}"
    micro="0"
    ver="${major}.${minor}${rev}"
  fi
  [ -n "$crev" ] && cver="${cver}.${crev}"
  echo "ver='$ver'"
  echo "major='$major'"
  echo "minor='$minor'"
  echo "micro='$micro'"
  echo "rev='$rev'"
  echo "cver='$cver'"
  echo "cmajor='$cmajor'"
  echo "cminor='$cminor'"
  echo "cmicro='$cmicro'"
  echo "crev='$crev'"
}

set_fs_ver () {
  local ver="$1" major="$2" minor="$3" micro="$4" rev="$5"
  sed -e "s|\(AC_SUBST(SWITCH_VERSION_MAJOR, \[\).*\(\])\)|\1$major\2|" \
    -e "s|\(AC_SUBST(SWITCH_VERSION_MINOR, \[\).*\(\])\)|\1$minor\2|" \
    -e "s|\(AC_SUBST(SWITCH_VERSION_MICRO, \[\).*\(\])\)|\1$micro\2|" \
    -e "s|\(AC_INIT(\[freeswitch\], \[\).*\(\], BUG-REPORT-ADDRESS)\)|\1$ver\2|" \
    -i configure.in
  if [ -n "$rev" ]; then
    sed -e "s|\(AC_SUBST(SWITCH_VERSION_REVISION, \[\).*\(\])\)|\1$rev\2|" \
      -e "s|#\(AC_SUBST(SWITCH_VERSION_REVISION\)|\1|" \
      -i configure.in
  fi
}

gnuize () {
  ./bootstrap.sh -j
  mv bootstrap.sh rebootstrap.sh
  rm -f docs/AUTHORS
  rm -f docs/COPYING
  rm -f docs/ChangeLog
  rm -rf .git
}

check_pwd () {
  if [ ! -d .git ]; then
    echo "error: must be run from within the top level of a FreeSWITCH git tree." 1>&2
    exit 1;
  fi
}

check_input_ver_build () {
  if [ -z "$1" ]; then
    echo "usage: $0 <version> <build-number>" 1>&2
    exit 1;
  fi
}


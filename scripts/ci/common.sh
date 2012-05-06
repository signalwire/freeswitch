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

if [ ! -d .git ]; then
  echo "error: must be run from within the top level of a FreeSWITCH git tree." 1>&2
  exit 1;
fi

if [ -z "$1" ]; then
  echo "usage: $0 <version> <build-number>" 1>&2
  exit 1;
fi


#!/bin/bash
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-
##### Author: Travis Cross <tc@traviscross.com>

pwd="$(pwd)"
ddir="debian"
[ "${pwd##*/}" = "debian" ] && ddir="."

err () {
  echo "$0 error: $1" >&2
  exit 1
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

list_build_depends () {
  test -f $ddir/.stamp-bootstrap || (cd $ddir && ./bootstrap.sh)
  local deps="" found=false
  while xread l; do
    if [ "${l%%:*}" = "Build-Depends" ]; then
      deps="${l#*:}"
      found=true
      continue
    elif $found; then
      if [ -z "$l" ]; then
        # is newline
        break
      elif [ -z "${l##\#*}" ]; then
        # is comment
        continue
      elif [ -z "${l## *}" ]; then
        # is continuation line
        deps="$deps $(echo "$l" | sed -e 's/^ *//' -e 's/ *([^)]*)//g' -e 's/,//g')"
      else
        # is a new header
        break
      fi
    fi
  done < $ddir/control
  echo "${deps# }"
}

cmd="$1"
shift
case "$cmd" in
  create-dbg-pkgs) create_dbg_pkgs ;;
  list-build-depends) list_build_depends ;;
esac


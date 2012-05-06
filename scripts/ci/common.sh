#!/bin/bash
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

src_repo="$(pwd)"
tmp_dir=${TMP_DIR:="/tmp"}

parse_version () {
  local ver="$1"
  local major=$(echo "$ver" | cut -d. -f1)
  local minor=$(echo "$ver" | cut -d. -f2)
  local micro=$(echo "$ver" | cut -d. -f3)
  local rev=$(echo "$ver" | cut -d. -f4)
  echo "ver='$ver'"
  echo "major='$major'"
  echo "minor='$minor'"
  echo "micro='$micro'"
  echo "rev='$rev'"
}

if [ ! -d .git ]; then
  echo "error: must be run from within the top level of a FreeSWITCH git tree." 1>&2
  exit 1;
fi

if [ -z "$1" ]; then
  echo "usage: $0 MAJOR.MINOR.MICRO[.REVISION] BUILD_NUMBER" 1>&2
  exit 1;
fi


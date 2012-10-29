#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

sdir="."
[ -n "${0%/*}" ] && sdir="${0%/*}"
. $sdir/../scripts/ci/common.sh

check_pwd
check_input_ver_build $@
in_ver="$1"
in_hrev="$2"
if [ "$in_ver" = "auto" ]; then
  in_ver="$(cat build/next-release.txt)"
fi
eval $(parse_version "$in_ver")
set_fs_ver "$gver" "$gmajor" "$gminor" "$gmicro" "$grev" "$in_hrev"


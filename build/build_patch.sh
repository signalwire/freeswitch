#!/bin/bash
set -e -o pipefail

usage () {
  printf "usage: %s [-u <remote>] [-r <ref>] [<patch-urls> ...]\n" "$0" >&2
}

err () {
  printf "error: %s\n" "$1" >&2
  exit 1
}

remote=origin
ref=origin/master
dopull=true
while getopts "hnr:u:" o; do
  case "$o" in
    h) usage; exit 0 ;;
    n) dopull=false ;;
    r) ref="$OPTARG" ;;
    u) remote="$OPTARG" ;;
  esac
done
shift $(($OPTIND-1))

if ! which git >/dev/null; then
  printf "error: please install git\n">&2
  exit 1; fi
if ! which wget >/dev/null; then
  printf "error: please install wget\n">&2
  exit 1; fi

now=$(date -u +%Y%m%dT%H%M%SZ)
git clean -fdx || err "failed"
git reset --hard "$ref" \
  || err "reset failed"
$dopull && (git pull "$remote" || err "failed to pull")
for patch in "$@"; do
  wget -O - "$patch" | git am
done
printf '# Building FreeSWITCH %s\n' "$(git describe HEAD)" \
  > ${now}-fsbuild.log
(./bootstrap.sh && ./configure -C && make VERBOSE=1) 2>&1 \
  | tee -a ${now}-fsbuild.log

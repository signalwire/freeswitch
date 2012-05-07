#!/bin/bash
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-
##### release a version of FreeSWITCH

sdir="."
[ -n "${0%/*}" ] && sdir="${0%/*}"
. $sdir/ci/common.sh

check_pwd

showusage() {
  cat >&2 <<EOF
SYNOPSIS
  $0 [-s] <version>

DESCRIPTION
  Creates a new FreeSWITCH tag after performing some sanity checks.
  The tag is optionally signed if '-s' is provided, but you should
  really sign any public release tags, so pass '-s'.

  <version> follows the format:
  
  1.2-alpha3
  1.2-beta3
  1.2-rc3
  1.2
  1.2.13-rc4
  1.2.13
  etc.

  This tool will take care of correctly naming the tag to be
  consistent with FreeSWITCH git policy from there.

OPTIONS
  -s
    Signs the resulting tag.

  -d
    Debug mode.  Remove the tag after creating it and don't warn about
    the lack of a signature.

EOF
  exit 1;
}

opts=""
debug=false
while getopts "ds" o; do
  case "$o" in
    d) debug=true ;;
    s) opts="-s" ;;
  esac
done
shift $(($OPTIND-1))

if [ -z "$1" ]; then
  showusage
fi

eval $(parse_version "$1")
ngrep () { (echo "$2" | grep -e "$1" >/dev/null); }
err () { echo "$1" >&2; exit 1; }

ngrep '^[1-9]*$' "$gmajor" || \
  err "The major version '$gmajor' appears invalid."
ngrep '^[0-9]*$' "$gminor" || \
  err "The minor version '$gminor' appears invalid."
[ -z "$gmicro" ] || ngrep '^[0-9]*$' "$gmicro" || \
  err "The micro version '$gmicro' appears invalid."
[ -z "$grev" ] || ngrep '^[.-]' "$grev" || \
  err "The revision '$grev' appears invalid."

echo "We're going to release freeswitch v$gver" >&2
echo >&2

if ! ($debug || ngrep '-s' "$opts"); then
  cat >&2 <<EOF
You've asked me to tag a release but haven't asked to me sign it by
passing -s.  I'll do this if you really want, but it's a bad idea if
you're making an actual release of FreeSWITCH that'll be seen
publicly.

EOF
  while true; do
    echo -n "Is this just a test tag? (yes/no): " >&2
    read r
    [ -z "$r" ] && continue
    if [ "$r" = yes ] || [ "$r" = y ]; then
      (echo; echo "OK, I believe you."; echo) >&2
      break
    else
      (echo; echo "This is a bad idea then."; echo) >&2
    fi
    while true; do
      echo -n "Are you really really sure? (yes/no): "  >&2
      read r
      [ -z "$r" ] && continue
      if [ "$r" = yes ] || [ "$r" = y ]; then
        (echo; echo "As you wish, you've been warned."; echo) >&2
        break
      else
        (echo; echo "Great; go setup a GPG key and try again with -s"; echo) >&2
        exit 1
      fi
      break
    done
    break
  done
fi

echo "Saving uncommitted changes before tagging..." >&2
ret=$(git stash save "Save uncommitted changes before tagging.")
if (ngrep '^Saved' "$ret"); then
  stash_saved=1
fi

echo "Changing the version of configure.in..." >&2
set_fs_ver "$gver" "$gmajor" "$gminor" "$gmicro" "$grev"

echo "Committing the new version..." >&2
git add configure.in
if ! (git commit --allow-empty -m "release FreeSWITCH $gver"); then
  cat >&2 <<EOF
Committing the new version failed for some reason.  Definitely look
into this before proceeding.

EOF
  err "Stopping here."
fi

echo "Tagging freeswitch v$gver..." >&2
if ! (git tag -a ${opts} -m "FreeSWITCH $gver" "v$gver"); then
  cat >&2 <<EOF
Committing the new tag failed for some reason.  Maybe you didn't
delete an old tag with this name?  Definitely figure out what's wrong
before proceeding.

EOF
  err "Stopping here."
fi

if $debug; then
  (echo; echo "We're in debug mode, so we're cleaning up...") >&2
  git tag -d "v$gver" || true
  git reset --hard HEAD^ || true
fi

if [ -n "$stash_saved" ]; then
  echo "Restoring your uncommitted changes to your working directory..." >&2
  git stash pop >/dev/null
fi

cat 1>&2 <<EOF
----------------------------------------------------------------------
The v$gver tag has been committed locally, but it will not be
globally visible until you 'git push --tags' this repository up to the
server (I didn't do that for you, as you might want to review first).

  Next step:

    git push --tags
----------------------------------------------------------------------
EOF

exit 0

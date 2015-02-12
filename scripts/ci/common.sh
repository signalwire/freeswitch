#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

src_repo="$(pwd)"
tmp_dir=${TMP_DIR:=".."}

zgrep () { (echo "$2" | grep -e "$1" >/dev/null); }

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
  # The major version should never be null
  if [ -z "$major" ]; then
    echo "WARNING: parse_version was called with '$1' which is missing a major version number" >&2
  fi
  # If someone asks for the minor or micro specificially, they
  # probably expect that it won't be null.  Also, vX.Y should never be
  # different from vX.Y.0 (that would be crazy), so we don't lose
  # meaningful generality by setting minor or micro to zero on vX or
  # vX.Y style versions.
  minor="${minor:-0}"
  micro="${micro:-0}"
  # centos-style versions (don't mess with the argument given for now)
  # TODO: what is the CentOS version number policy?
  local cmajor cminor cmicro crev cver
  cmajor="${major:-0}"
  cminor="${minor:-0}"
  cmicro="${micro:-0}"
  crev="$(echo "$rev" | sed -e 's/[._~-]//')"
  cver="${cmajor}.${cminor}.${cmicro}"
  [ -n "$crev" ] && cver="${cver}.${crev}"
  # fix up if the revision was passed in the minor or micro number
  if zgrep '^\(alpha\|beta\|rc\)' "$minor"; then
    rev="-${minor}"
    minor="0"
    micro="0"
    ver="${major}${rev}"
  fi
  if zgrep '^\(alpha\|beta\|rc\)' "$micro"; then
    rev="-${micro}"
    micro="0"
    ver="${major}.${minor}${rev}"
  fi
  # git-style versions
  local gmajor gminor gmicro grev gver
  gver="$(echo "$ver" | sed -e 's/[~_]/-/')"
  grev="$(echo "$rev" | sed -e 's/[~_]/-/')"
  gmajor="$major"
  gminor="$minor"
  gmicro="$micro"
  # debian-style versions
  local dmajor dminor dmicro drev dver
  dver="$(echo "$ver" | sed -e 's/[-_]/~/')"
  drev="$(echo "$rev" | sed -e 's/[-_]/~/')"
  dmajor="$major"
  dminor="$minor"
  dmicro="$micro"
  # return variables
  echo "ver='$ver'"
  echo "major='$major'"
  echo "minor='$minor'"
  echo "micro='$micro'"
  echo "rev='$rev'"
  echo "gver='$gver'"
  echo "gmajor='$gmajor'"
  echo "gminor='$gminor'"
  echo "gmicro='$gmicro'"
  echo "grev='$grev'"
  echo "dver='$dver'"
  echo "dmajor='$dmajor'"
  echo "dminor='$dminor'"
  echo "dmicro='$dmicro'"
  echo "drev='$drev'"
  echo "cver='$cver'"
  echo "cmajor='$cmajor'"
  echo "cminor='$cminor'"
  echo "cmicro='$cmicro'"
  echo "crev='$crev'"
}

set_fs_ver () {
  local ver="$1" major="$2" minor="$3" micro="$4" rev="$5" hrev="$6"
  sed  \
    -e "s|\(AC_SUBST(SWITCH_VERSION_MAJOR, \[\).*\(\])\)|\1$major\2|" \
    -e "s|\(AC_SUBST(SWITCH_VERSION_MINOR, \[\).*\(\])\)|\1$minor\2|" \
    -e "s|\(AC_SUBST(SWITCH_VERSION_MICRO, \[\).*\(\])\)|\1$micro\2|" \
    -e "s|\(AC_INIT(\[freeswitch\], \[\).*\(\], bugs@freeswitch.org)\)|\1$ver\2|" \
    configure.ac > configure.ac.$$
    mv configure.ac.$$ configure.ac
  if [ -n "$rev" ]; then
    [ -n "$hrev" ] || hrev="$rev"
    sed -e "s|\(AC_SUBST(SWITCH_VERSION_REVISION, \[\).*\(\])\)|\1$rev\2|" \
      -e "s|\(AC_SUBST(SWITCH_VERSION_REVISION_HUMAN, \[\).*\(\])\)|\1'$hrev'\2|" \
      -e "s|#\(AC_SUBST(SWITCH_VERSION_REVISION\)|\1|" \
      -e "s|#\(AC_SUBST(SWITCH_VERSION_REVISION_HUMAN\)|\1|" \
      configure.ac > configure.ac.$$
      mv configure.ac.$$ configure.ac
  fi

  local rpm_version
  if [ -n "$rev" ]; then
    rpm_version="$major.$minor.$micro.$rev"
  else
    rpm_version="$major.$minor.$micro"
  fi

  sed -e "s|\(%define version \).*|\1$rpm_version|"  \
       freeswitch.spec > freeswitch.spec.$$
  mv freeswitch.spec.$$ freeswitch.spec

  sed -e "s|\(%define version \).*|\1$rpm_version|"  \
       freeswitch-config-rayo.spec > freeswitch-config-rayo.spec.$$
  mv freeswitch-config-rayo.spec.$$ freeswitch-config-rayo.spec

#%define version 1.5.16

}

set_fs_release () {
  local release="$1"
  if [ -n "$release" ]; then
    sed -e "s|\(%define release \).*|\1$release|"  \
         freeswitch.spec > freeswitch.spec.$$
    mv freeswitch.spec.$$ freeswitch.spec

    sed -e "s|\(%define release \).*|\1$release|"  \
         freeswitch-config-rayo.spec > freeswitch-config-rayo.spec.$$
    mv freeswitch-config-rayo.spec.$$ freeswitch-config-rayo.spec
  fi
}

gnuize () {
  ./bootstrap.sh
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


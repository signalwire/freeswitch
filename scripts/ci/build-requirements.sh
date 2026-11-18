#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

check_ac_ver() {
  # autoconf 2.59 or newer
  ac_version=`${AUTOCONF:-autoconf} --version 2>/dev/null|sed -e 's/^[^0-9]*//;s/[a-z]* *$//;s/[- ].*//g;q'`
  if test -z "$ac_version"; then
    echo "build-requirements: autoconf not found."
    echo "                    You need autoconf version 2.59 or newer installed"
    echo "                    to build FreeSWITCH from source."
    exit 1
  fi
  if test `uname -s` = "OpenBSD" && test "$ac_version" = "2.62"; then
    echo "Autoconf 2.62 is broken on OpenBSD, please try another version"
    exit 1
  fi
  IFS=_; set $ac_version; IFS=' '
  ac_version=$1
  IFS=.; set $ac_version; IFS=' '
  if test "$1" = "2" -a "$2" -lt "59" || test "$1" -lt "2"; then
    echo "build-requirements: autoconf version $ac_version found."
    echo "                    You need autoconf version 2.59 or newer installed"
    echo "                    to build FreeSWITCH from source."
    exit 1
  else
    echo "build-requirements: autoconf version $ac_version (ok)"
  fi
}

check_am_ver() {
  # automake 1.7 or newer
  am_version=`${AUTOMAKE:-automake} --version 2>/dev/null|sed -e 's/^[^0-9]*//;s/[a-z]* *$//;s/[- ].*//g;q'`
  if test -z "$am_version"; then
    echo "build-requirements: automake not found."
    echo "                    You need automake version 1.7 or newer installed"
    echo "                    to build FreeSWITCH from source."
    exit 1
  fi
  IFS=_; set $am_version; IFS=' '
  am_version=$1
  IFS=.; set $am_version; IFS=' '
  if test "$1" = "1" -a "$2" -lt "7"; then
    echo "build-requirements: automake version $am_version found."
    echo "                    You need automake version 1.7 or newer installed"
    echo "                    to build FreeSWITCH from source."
    exit 1
  else
    echo "build-requirements: automake version $am_version (ok)"
  fi
}

check_acl_ver() {
  # aclocal 1.7 or newer
  acl_version=`${ACLOCAL:-aclocal} --version 2>/dev/null|sed -e 's/^[^0-9]*//;s/[a-z]* *$//;s/[- ].*//g;q'`
  if test -z "$acl_version"; then
    echo "build-requirements: aclocal not found."
    echo "                    You need aclocal version 1.7 or newer installed"
    echo "                    to build FreeSWITCH from source."
    exit 1
  fi
  IFS=_; set $acl_version; IFS=' '
  acl_version=$1
  IFS=.; set $acl_version; IFS=' '
  if test "$1" = "1" -a "$2" -lt "7"; then
    echo "build-requirements: aclocal version $acl_version found."
    echo "                    You need aclocal version 1.7 or newer installed"
    echo "                    to build FreeSWITCH from source."
    exit 1
  else
    echo "build-requirements: aclocal version $acl_version (ok)"
  fi
}

check_lt_ver() {
  # Sample libtool --version outputs:
  # ltmain.sh (GNU libtool) 1.3.3 (1.385.2.181 1999/07/02 15:49:11)
  # ltmain.sh (GNU libtool 1.1361 2004/01/02 23:10:52) 1.5a
  # output is multiline from 1.5 onwards

  # find APR PrintPath binary path
  for el in "${LIBDIR}/apr/build/PrintPath" \
            "libs/apr/build/PrintPath" \
            "../libs/apr/build/PrintPath" \
            "../../libs/apr/build/PrintPath"; do
    if test -f "$el"; then
      PrintPathBin="$el"
      break
    fi
  done

  # Require libtool 1.4 or newer
  libtool=${LIBTOOL:-`${PrintPathBin} glibtool libtool libtool22 libtool15 libtool14`}
  lt_pversion=`$libtool --version 2>/dev/null|sed -e 's/([^)]*)//g;s/^[^0-9]*//;s/[- ].*//g;q'`
  if test -z "$lt_pversion"; then
    echo "build-requirements: libtool not found."
    echo "                    You need libtool version 1.5.14 or newer to build FreeSWITCH from source."
    exit 1
  fi
  lt_version=`echo $lt_pversion|sed -e 's/\([a-z]*\)$/.\1/'`
  IFS=.; set $lt_version; IFS=' '
  lt_status="good"

  if test -z "$1"; then a=0 ; else a=$1;fi
  if test -z "$2"; then b=0 ; else b=$2;fi
  if test -z "$3"; then c=0 ; else c=$3;fi
  lt_major=$a

  if test "$a" -eq "2"; then
    lt_status="good"
  elif test "$a" -lt "2"; then
    if test "$b" -lt "5" -o "$b" =  "5" -a "$c" -lt "14" ; then
      lt_status="bad"
    fi
  else
    lt_status="bad"
  fi
  if test $lt_status = "good"; then
    echo "build-requirements: libtool version $lt_pversion (ok)"
  else
    echo "build-requirements: libtool version $lt_pversion found."
    echo "                    You need libtool version 1.5.14 or newer to build FreeSWITCH from source."
    exit 1
  fi
}

check_libtoolize() {
  # find APR PrintPath binary path
  for el in "${LIBDIR}/apr/build/PrintPath" \
            "libs/apr/build/PrintPath" \
            "../libs/apr/build/PrintPath" \
            "../../libs/apr/build/PrintPath"; do
    if test -f "$el"; then
      PrintPathBin="$el"
      break
    fi
  done

  # check libtoolize availability
  if [ -n "${LIBTOOL}" ]; then
    libtoolize=${LIBTOOLIZE:-`dirname "${LIBTOOL}"`/libtoolize}
  else
    libtoolize=${LIBTOOLIZE:-`${PrintPathBin} glibtoolize libtoolize libtoolize22 libtoolize15 libtoolize14`}
  fi
  if [ "x$libtoolize" = "x" ]; then
    echo "libtoolize not found in path"
    exit 1
  fi
  if [ ! -x "$libtoolize" ]; then
    echo "$libtoolize does not exist or is not executable"
    exit 1
  fi

  # compare libtool and libtoolize version
  ltl_pversion=`$libtoolize --version 2>/dev/null|sed -e 's/([^)]*)//g;s/^[^0-9]*//;s/[- ].*//g;q'`
  ltl_version=`echo $ltl_pversion|sed -e 's/\([a-z]*\)$/.\1/'`
  IFS=.; set $ltl_version; IFS=' '

  if [ "x${lt_version}" != "x${ltl_version}" ]; then
    echo "$libtool and $libtoolize have different versions"
    exit 1
  fi
}

check_make() {
  #
  # Check to make sure we have GNU Make installed
  #

  make=`which make`
  if [ -x "$make" ]; then
     make_version=`$make --version || true | grep GNU`
     if [ $? -ne 0 ]; then
        make=`which gmake`
        if [ -x "$make" ]; then
          make_version=`$make --version | grep GNU`
          if [ $? -ne 0 ]; then
            echo "GNU Make does not exist or is not executable"
            exit 1;
          fi
        fi
      fi
   fi
}

check_awk() {
  # TODO: Building with mawk on at least Debian squeeze is know to
  # work, but mawk is believed to fail on some systems.  If we can
  # replicate this, we need a particular behavior that we can test
  # here to verify whether we have an acceptable awk.
  :
}

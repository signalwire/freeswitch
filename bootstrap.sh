#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-
##### bootstrap FreeSWITCH and FreeSWITCH libraries

. $(dirname $0)/scripts/ci/build-requirements.sh

echo "bootstrap: checking installation..."

BGJOB=false
VERBOSE=false
BASEDIR=`pwd`;
LIBDIR=${BASEDIR}/libs;
SUBDIRS="apr libzrtp iksemel libdingaling srtp freetdm unimrcp fs";

while getopts 'jhd:v' o; do 
  case "$o" in
    j) BGJOB=true;;
    d) SUBDIRS="$OPTARG";;
    v) VERBOSE=true;;
    h) echo "Usage: $0 <options>"
      echo "  Options:"
      echo "           -d 'library1 library2'"
      echo "             => Bootstrap only specified subdirectories"
      echo "           -j => Run Jobs in Background"
      exit;;
  esac
done

ex() {
  test $VERBOSE && echo "bootstrap: $@" >&2
  $@
}

setup_modules() {
  if [ ! -f modules.conf ]; then 
    cp build/modules.conf.in modules.conf
  fi
}

setup_gnu() {
  # keep automake from making us magically GPL, and to stop
  # complaining about missing files.
  cp -f docs/COPYING .
  cp -f docs/AUTHORS .
  cp -f docs/ChangeLog .
  touch NEWS
  touch README
}

print_autotools_vers() {
  #
  # Info output
  #
  echo "Bootstrapping using:"
  echo "  autoconf  : ${AUTOCONF:-`which autoconf`}"
  echo "  automake  : ${AUTOMAKE:-`which automake`}"
  echo "  aclocal   : ${ACLOCAL:-`which aclocal`}"
  echo "  libtool   : ${libtool} (${lt_version})"
  echo "  libtoolize: ${libtoolize}"
  echo "  make      : ${make} (${make_version})"
  echo "  awk       : ${awk} (${awk_version})"
  echo
}

bootstrap_apr() {
  echo "Entering directory ${LIBDIR}/apr"
  cd ${LIBDIR}/apr

  # Licensed to the Apache Software Foundation (ASF) under one or more
  # contributor license agreements.  See the NOTICE file distributed with
  # this work for additional information regarding copyright ownership.
  # The ASF licenses this file to You under the Apache License, Version 2.0
  # (the "License"); you may not use this file except in compliance with
  # the License.  You may obtain a copy of the License at
  # 
  #    http://www.apache.org/licenses/LICENSE-2.0
  #
  # Unless required by applicable law or agreed to in writing, software
  # distributed under the License is distributed on an "AS IS" BASIS,
  # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  # See the License for the specific language governing permissions and
  # limitations under the License.
  #
  #

  # bootstrap: Build the support scripts needed to compile from a
  #            checked-out version of the source code.

  # Create the libtool helper files
  #
  # Note: we copy (rather than link) them to simplify distribution.
  # Note: APR supplies its own config.guess and config.sub -- we do not
  #       rely on libtool's versions
  #
  echo "Copying libtool helper files ..."

  # Remove any libtool files so one can switch between libtool 1.3
  # and libtool 1.4 by simply rerunning the bootstrap script.
  (cd build ; rm -f ltconfig ltmain.sh libtool.m4)

  if ${libtoolize} -n --install >/dev/null 2>&1 ; then
    $libtoolize --force --copy --install
  else
    $libtoolize --force --copy
  fi

  if [ -f libtool.m4 ]; then 
    ltfile=`pwd`/libtool.m4
  else
    if [ $lt_major -eq 2 ]; then
      ltfindcmd="`sed -n \"/aclocaldir=/{s/.*=/echo /p;q;}\" < $libtoolize`"
      ltfile=${LIBTOOL_M4-`eval "$ltfindcmd"`/libtool.m4}
    else
      ltfindcmd="`sed -n \"/=[^\\\`]/p;/libtool_m4=/{s/.*=/echo /p;q;}\" \
                     < $libtoolize`"
      ltfile=${LIBTOOL_M4-`eval "$ltfindcmd"`}
    fi
     # Expecting the code above to be very portable, but just in case...
    if [ -z "$ltfile" -o ! -f "$ltfile" ]; then
      ltpath=`dirname $libtoolize`
      ltfile=`cd $ltpath/../share/aclocal ; pwd`/libtool.m4
    fi
  fi

  if [ ! -f $ltfile ]; then
    echo "$ltfile not found"
    exit 1
  fi

  echo "bootstrap: Using libtool.m4 at ${ltfile}."

  cat $ltfile | sed -e 's/LIBTOOL=\(.*\)top_build/LIBTOOL=\1apr_build/' > build/libtool.m4

  # libtool.m4 from 1.6 requires ltsugar.m4
  if [ -f ltsugar.m4 ]; then
    rm -f build/ltsugar.m4
    mv ltsugar.m4 build/ltsugar.m4
  fi

  # Clean up any leftovers
  rm -f aclocal.m4 libtool.m4

  # fix for FreeBSD (at least):
  # libtool.m4 is in share/aclocal, while e.g. aclocal19 only looks in share/aclocal19
  # get aclocal's default directory and include the libtool.m4 directory via -I if
  # it's in a different location

  aclocal_dir="`${ACLOCAL:-aclocal} --print-ac-dir`"

  if [ -n "${aclocal_dir}" -a -n "${ltfile}" -a "`dirname ${ltfile}`" != "${aclocal_dir}" ] ; then
    ACLOCAL_OPTS="-I `dirname ${ltfile}`"
  fi

  ### run aclocal
  echo "Re-creating aclocal.m4 ..."
  ${ACLOCAL:-aclocal} ${ACLOCAL_OPTS}

  ### do some work to toss config.cache?
  rm -rf config.cache

  echo "Creating configure ..."
  ${AUTOCONF:-autoconf}

  #
  # Generate the autoconf header
  #
  echo "Creating include/arch/unix/apr_private.h.in ..."
  ${AUTOHEADER:-autoheader}

  # Remove autoconf 2.5x's cache directory
  rm -rf autom4te*.cache

  echo "Entering directory ${LIBDIR}/apr-util"
  cd ${LIBDIR}/apr-util
  ./buildconf
}

bootstrap_libzrtp() {
  (cd ${LIBDIR}/libzrtp && ./bootstrap.sh)
}

# Libs automake automation function
libbootstrap() {
  i=$1
  if [ -d ${LIBDIR}/${i} ]; then
    echo "Entering directory ${LIBDIR}/${i}"
    ex cd ${LIBDIR}/${i}
    ex rm -f aclocal.m4
    CFFILE=
    if [ -f ${LIBDIR}/${i}/configure.in ]; then
      CFFILE="${LIBDIR}/${i}/configure.in"
    else
      if [ -f ${LIBDIR}/${i}/configure.ac ]; then
        CFFILE="${LIBDIR}/${i}/configure.ac"
      fi
    fi

    if [ ! -z ${CFFILE} ]; then
      LTTEST=`grep "AC_PROG_LIBTOOL" ${CFFILE}`
      LTTEST2=`grep "AM_PROG_LIBTOOL" ${CFFILE}`
      AMTEST=`grep "AM_INIT_AUTOMAKE" ${CFFILE}`
      AMTEST2=`grep "AC_PROG_INSTALL" ${CFFILE}`
      AHTEST=`grep "AC_CONFIG_HEADERS" ${CFFILE}`
      AXTEST=`grep "ACX_LIBTOOL_C_ONLY" ${CFFILE}`

      echo "Creating aclocal.m4"
      ex ${ACLOCAL:-aclocal} ${ACLOCAL_OPTS} ${ACLOCAL_FLAGS}

      # only run if AC_PROG_LIBTOOL is in configure.in/configure.ac
      if [ ! -z "${LTTEST}" -o "${LTTEST2}" -o "${AXTEST}" ]; then
        echo "Running libtoolize..."
        if ${libtoolize} -n --install >/dev/null 2>&1; then
          ex $libtoolize --force --copy --install
        else
          ex $libtoolize --force --copy
        fi
      fi

      echo "Creating configure"
      ex ${AUTOCONF:-autoconf}

      # only run if AC_CONFIG_HEADERS is found in configure.in/configure.ac
      if [ ! -z "${AHTEST}" ]; then
        echo "Running autoheader..."
        ex ${AUTOHEADER:-autoheader};
      fi

      # run if AM_INIT_AUTOMAKE / AC_PROG_INSTALL is in configure.in/configure.ac
      if [ ! -z "${AMTEST}" -o "${AMTEST2}" ]; then
        echo "Creating Makefile.in"
        ex ${AUTOMAKE:-automake} --no-force --add-missing --copy;
      fi
      ex rm -rf autom4te*.cache
    fi
  else
    echo "Skipping directory ${LIBDIR}/${i}"
  fi
}

bootstrap_fs() {
  cd ${BASEDIR}
  rm -f aclocal.m4
  ${ACLOCAL:-aclocal} ${ACLOCAL_OPTS}
  $libtoolize --copy --automake
  ${AUTOCONF:-autoconf}
  ${AUTOHEADER:-autoheader}
  ${AUTOMAKE:-automake} --no-force --add-missing --copy
  rm -rf autom4te*.cache
}

bootstrap_libs_pre() {
  case "$1" in
    *) return 0;;
  esac
}

bootstrap_libs_post() {
  case "$1" in
    *) return 0;;
  esac
}

bootstrap_libs() {
  for i in ${SUBDIRS}; do
    case "$i" in
      apr|fs|libzrtp)
        ${BGJOB} && wait
        bootstrap_$i
        continue
        ;;
    esac
    bootstrap_libs_pre ${i}
    if ! ${BGJOB}; then
      libbootstrap ${i} ; bootstrap_libs_post ${i}
    else
      (libbootstrap ${i} ; bootstrap_libs_post ${i}) &
    fi
  done
  ${BGJOB} && wait
}

run() {
  setup_modules
  setup_gnu
  check_make
  check_awk
  check_ac_ver
  check_am_ver
  check_acl_ver
  check_lt_ver
  check_libtoolize
  print_autotools_vers
  bootstrap_libs
  return 0
}

run


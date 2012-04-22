dnl =====================================================================
dnl This file contains autoconf macros shared by Sofia modules.
dnl
dnl Author: Pekka Pessi <Pekka.Pessi@nokia.com>
dnl
dnl License:
dnl
dnl Copyright (c) 2001,2004 Nokia and others. All Rights Reserved.
dnl
dnl Please note that every macro contained in this file is copyrighted by
dnl its respective author, unless the macro source explicitely says
dnl otherwise. Permission has been granted, though, to use and distribute
dnl all macros under the following license, which is a modified version of
dnl the GNU General Public License version 2:
dnl
dnl Each Autoconf macro in this file is free software; you can redistribute it
dnl and/or modify it under the terms of the GNU General Public License as
dnl published by the Free Software Foundation; either version 2, or (at your
dnl option) any later version.
dnl
dnl They are distributed in the hope that they will be useful, but WITHOUT ANY
dnl WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
dnl FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
dnl details. (You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software Foundation,
dnl Inc., 59 Temple Place -- Suite 330, Boston, MA 02111-1307, USA.)
dnl
dnl As a special exception, the Free Software Foundation gives unlimited
dnl permission to copy, distribute and modify the configure scripts that are
dnl the output of Autoconf. You need not follow the terms of the GNU General
dnl Public License when using or distributing such scripts, even though
dnl portions of the text of Autoconf appear in them. The GNU General Public
dnl License (GPL) does govern all other use of the material that constitutes
dnl the Autoconf program.
dnl
dnl Certain portions of the Autoconf source text are designed to be copied
dnl (in certain cases, depending on the input) into the output of Autoconf.
dnl We call these the "data" portions. The rest of the Autoconf source text
dnl consists of comments plus executable code that decides which of the data
dnl portions to output in any given case. We call these comments and
dnl executable code the "non-data" portions. Autoconf never copies any of
dnl the non-data portions into its output.
dnl
dnl This special exception to the GPL applies to versions of Autoconf
dnl released by the Free Software Foundation. When you make and distribute a
dnl modified version of Autoconf, you may extend this special exception to
dnl the GPL to apply to your modified version as well, *unless* your
dnl modified version has the potential to copy into its output some of the
dnl text that was the non-data portion of the version that you started with.
dnl (In other words, unless your change moves or copies text from the
dnl non-data portions to the data portions.) If your modification has such
dnl potential, you must delete any notice of this special exception to the
dnl GPL from your modified version.
dnl
dnl =====================================================================

dnl ===================================================================
dnl Define --enable-coverage, enable coverage in Makefile.am
dnl ===================================================================

AC_DEFUN([SAC_COVERAGE], [

AC_REQUIRE([AC_PROG_CC])

#
# GCoverage
#
AC_ARG_ENABLE(coverage,
[  --enable-coverage       compile test-coverage [[disabled]]],
 , enable_coverage=no)

if test X$enable_coverage = Xno ; then
:
elif test X$GCC != Xyes ; then
  AC_MSG_ERROR([--enable-coverage requires gcc])
else
  COVERAGE_FLAGS="-fprofile-arcs -ftest-coverage"

dnl old_CFLAGS=$CFLAGS
dnl CFLAGS="$CFLAGS -fprofile-arcs -ftest-coverage"
dnl AC_SEARCH_LIBS(__gcov_init, gcov)
dnl CFLAGS=$old_CFLAGS
dnl
dnl libtool 1.5.22 and lower strip -fprofile-arcs from the flags
dnl passed to the linker, which is a bug; -fprofile-arcs implicitly
dnl links in -lgcov, so we do it explicitly here for the same effect
dnl
  AC_CHECK_LIB(gcov, __gcov_init)
  dnl GCOV is part of GCC suite
  GCOV=`echo $CC | sed s/gcc/gcov/g`
  AC_DEFINE([HAVE_COVERAGE], 1,
    [Defined when gcov is enabled to force by changing config.h])

  dnl Check for lcov utility
  AC_CHECK_PROG([LCOV], [lcov], [lcov], [false])
  if test X$LCOV != Xfalse ; then
    AC_MSG_CHECKING([whether $LCOV accepts --compat-libtool])
    if $LCOV --compat-libtool --help > /dev/null 2>&1 ; then
      AC_MSG_RESULT(ok)
    else
      AC_MSG_RESULT(not supported)
      AC_MSG_WARN([lcov option --compat-libtool is not supported])
      AC_MSG_WARN([Update lcov to version > 1.5])
      LCOV=false
    fi
  fi
  AC_CHECK_PROG([GENHTML], [genhtml], [genhtml], [false])
  AC_CHECK_PROG([GENPNG], [genpng], [genpng], [false])
  if test X$LCOV != Xfalse ; then
    AC_MSG_CHECKING([whether $GENPNG has all required modules])
    if $GENPNG --help > /dev/null 2>&1 ; then
      AC_MSG_RESULT(ok)
    else
      AC_MSG_RESULT(not supported)
      AC_MSG_WARN([GD.pm perl module is not installed])
      GENPNG=false
    fi
  fi
fi

AM_CONDITIONAL([ENABLE_COVERAGE], test X$enable_coverage != Xno)
AM_CONDITIONAL([HAVE_LCOV], test X$LCOV != Xfalse)
AM_CONDITIONAL([HAVE_GENPNG], test X$GENPNG != Xfalse)

AC_SUBST([GCOV])
AC_SUBST([COVERAGE_FLAGS])
AC_SUBST([MOSTLYCLEANFILES], "*.bb *.bbg *.da *.gcov *.gcda *.gcno")

])
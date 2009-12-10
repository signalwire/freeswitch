#
# g722_1 - a library for the G.722_1 codec
#
# configure.ac -- Process this file with autoconf to produce configure
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2, as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $Id: configure.ac,v 1.9 2008/10/09 14:17:12 steveu Exp $

# @start 1

AC_INIT

CFLAGS="$CFLAGS $CONFIGURE_CFLAGS"
CXXFLAGS="$CXXFLAGS $CONFIGURE_CXXFLAGS"
LDFLAGS="$LDFLAGS $CONFIGURE_LDFLAGS"

m4_include(config/ax_compiler_vendor.m4)
m4_include(config/ax_check_real_file.m4)
m4_include(config/ax_fixed_point_machine.m4)
m4_include(config/ax_misaligned_access_fails.m4)
m4_include(config/ax_c99_features.m4)

G722_1_MAJOR_VERSION=0
G722_1_MINOR_VERSION=0
G722_1_MICRO_VERSION=1

G722_1_LT_CURRENT=0
G722_1_LT_REVISION=2
G722_1_LT_AGE=0

VERSION=$G722_1_MAJOR_VERSION.$G722_1_MINOR_VERSION.$G722_1_MICRO_VERSION
PACKAGE=g722_1

AC_SUBST(G722_1_LT_CURRENT)
AC_SUBST(G722_1_LT_REVISION)
AC_SUBST(G722_1_LT_AGE)

AC_CONFIG_SRCDIR([src/encoder.c])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADERS([src/config.h:config-h.in])
AM_INIT_AUTOMAKE($PACKAGE, $VERSION)

AC_CANONICAL_HOST
#AC_CANONICAL_BUILD
AC_PROG_CC
AC_PROG_CXX
AC_PROG_GCC_TRADITIONAL
AC_PROG_LIBTOOL
AC_LANG([C])

AX_COMPILER_VENDOR

if test "${build}" != "${host}"
then
  # If we are doing a Canadian Cross, in which the host and build systems
  # are not the same, we set reasonable default values for the tools.

  CC_FOR_BUILD=${CC_FOR_BUILD-gcc}
  CPPFLAGS_FOR_BUILD="\$(CPPFLAGS)"
  CC=${CC-${host_alias}-gcc}
  CFLAGS=${CFLAGS-"-g -O2"}
  CXX=${CXX-${host_alias}-c++}
  CXXFLAGS=${CXXFLAGS-"-g -O2"}
else
  # Set reasonable default values for some tools even if not Canadian.
  # Of course, these are different reasonable default values, originally
  # specified directly in the Makefile.
  # We don't export, so that autoconf can do its job.
  # Note that all these settings are above the fragment inclusion point
  # in Makefile.in, so can still be overridden by fragments.
  # This is all going to change when we autoconfiscate...
  CC_FOR_BUILD="\$(CC)"
  CPPFLAGS_FOR_BUILD="\$(CPPFLAGS)"
  AC_PROG_CC

  # We must set the default linker to the linker used by gcc for the correct
  # operation of libtool.  If LD is not defined and we are using gcc, try to
  # set the LD default to the ld used by gcc.
  if test -z "$LD"
  then
    if test "$GCC" = yes
    then
      case $build in
      *-*-mingw*)
        gcc_prog_ld=`$CC -print-prog-name=ld 2>&1 | tr -d '\015'` ;;
      *)
        gcc_prog_ld=`$CC -print-prog-name=ld 2>&1` ;;
      esac
      case $gcc_prog_ld in
      # Accept absolute paths.
      [[\\/]* | [A-Za-z]:[\\/]*)]
        LD="$gcc_prog_ld" ;;
      esac
    fi
  fi

  CXX=${CXX-"c++"}
  CFLAGS=${CFLAGS-"-g -O2"}
  CXXFLAGS=${CXXFLAGS-"-g -O2"}
fi

AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE

AC_CHECK_TYPES(long long)
AC_CHECK_TYPES(long double)

AC_TYPE_SIGNAL

AC_ARG_ENABLE(doc,          [  --enable-doc         Build the documentation])
AC_ARG_ENABLE(tests,        [  --enable-tests       Build the test programs])
AC_ARG_ENABLE(mmx,          [  --enable-mmx         Enable MMX support])
AC_ARG_ENABLE(sse,          [  --enable-sse         Enable SSE support])
AC_ARG_ENABLE(sse2,         [  --enable-sse2        Enable SSE2 support])
AC_ARG_ENABLE(sse3,         [  --enable-sse3        Enable SSE3 support])
AC_ARG_ENABLE(sse3,         [  --enable-sse3        Enable SSE3 support])
AC_ARG_ENABLE(sse4_1,       [  --enable-sse4-1      Enable SSE4.1 support])
AC_ARG_ENABLE(sse4_2,       [  --enable-sse4-2      Enable SSE4.2 support])
AC_ARG_ENABLE(sse4a,        [  --enable-sse4a       Enable SSE4A support])
AC_ARG_ENABLE(sse5,         [  --enable-sse5        Enable SSE5 support])
AC_ARG_ENABLE(fixed_point,  [  --enable-fixed-point Enable fixed point support])

AC_FUNC_ERROR_AT_LINE
AC_FUNC_VPRINTF
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES

AX_C99_FUNC_LRINT
AX_C99_FUNC_LRINTF
AX_C99_FUNC_LLRINT
AX_C99_FUNC_LLRINTF

if test "x$ac_cv_c99_lrint" = "xno" ; then
    if test "x$ac_cv_c99_lrintf" = "xno" ; then
        AC_MSG_WARN([[*** Missing C99 standard functions lrint() and lrintf().]])
        AC_MSG_WARN([[*** This may cause benign compiler warnings on some systems (ie Solaris).]])
    fi
fi

AX_C99_FLEXIBLE_ARRAY

AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([select])
AC_CHECK_FUNCS([strcasecmp])
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([strstr])
AC_CHECK_FUNCS([strtol])
AC_CHECK_FUNCS([gettimeofday])

AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_TIME

# Check for header files.
AC_CHECK_HEADERS([socket.h])
AC_CHECK_HEADERS([inttypes.h], [INSERT_INTTYPES_HEADER="#include <inttypes.h>"])
AC_CHECK_HEADERS([stdint.h], [INSERT_STDINT_HEADER="#include <stdint.h>"])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([strings.h])
AC_CHECK_HEADERS([malloc.h])
AC_CHECK_HEADERS([tgmath.h], [INSERT_TGMATH_HEADER="#include <tgmath.h>"])
AC_CHECK_HEADERS([math.h], [INSERT_MATH_HEADER="#include <math.h>"])
AC_CHECK_HEADERS([float.h])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([sys/select.h])
AC_CHECK_HEADERS([sys/ioctl.h])
AC_CHECK_HEADERS([sys/fcntl.h])
AC_CHECK_HEADERS([audiofile.h])

AC_LANG([C])

if test "${build}" == "${host}"
then
    case "${host}" in
    x86_64-*)
        AX_CHECK_REAL_FILE([${prefix}/lib64], libdir='$(exec_prefix)/lib64')
        ;;
    esac
fi

# Checks for libraries.
AC_CHECK_LIB([m], [cos])
# Some platforms still seem to lack the basic single precision trig and power related function.
AC_SEARCH_LIBS([sinf], [m], AC_DEFINE([HAVE_SINF], [1], [Define to 1 if you have the sinf() function.]))
AC_SEARCH_LIBS([cosf], [m], AC_DEFINE([HAVE_COSF], [1], [Define to 1 if you have the cosf() function.]))
AC_SEARCH_LIBS([tanf], [m], AC_DEFINE([HAVE_TANF], [1], [Define to 1 if you have the tanf() function.]))
AC_SEARCH_LIBS([asinf], [m], AC_DEFINE([HAVE_ASINF], [1], [Define to 1 if you have the asinf() function.]))
AC_SEARCH_LIBS([acosf], [m], AC_DEFINE([HAVE_ACOSF], [1], [Define to 1 if you have the acosf() function.]))
AC_SEARCH_LIBS([atanf], [m], AC_DEFINE([HAVE_ATANF], [1], [Define to 1 if you have the atanf() function.]))
AC_SEARCH_LIBS([atan2f], [m], AC_DEFINE([HAVE_ATAN2F], [1], [Define to 1 if you have the atan2f() function.]))
AC_SEARCH_LIBS([ceilf], [m], AC_DEFINE([HAVE_CEILF], [1], [Define to 1 if you have the ceilf() function.]))
AC_SEARCH_LIBS([floorf], [m], AC_DEFINE([HAVE_FLOORF], [1], [Define to 1 if you have the floorf() function.]))
AC_SEARCH_LIBS([powf], [m], AC_DEFINE([HAVE_POWF], [1], [Define to 1 if you have the powf() function.]))
AC_SEARCH_LIBS([expf], [m], AC_DEFINE([HAVE_EXPF], [1], [Define to 1 if you have the expf() function.]))
AC_SEARCH_LIBS([logf], [m], AC_DEFINE([HAVE_LOGF], [1], [Define to 1 if you have the logf() function.]))
AC_SEARCH_LIBS([log10f], [m], AC_DEFINE([HAVE_LOG10F], [1], [Define to 1 if you have the log10f() function.]))
if test -n "$enable_tests" ; then
    AC_CHECK_LIB([audiofile], [afOpenFile], TESTLIBS="$TESTLIBS -laudiofile", AC_MSG_ERROR("Can't make tests without libaudiofile (does your system require a libaudiofile-devel package?)"))
fi

case "${ax_cv_c_compiler_vendor}" in
gnu)
    COMP_VENDOR_CFLAGS="-std=gnu99 -ffast-math -Wall -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes"
    if test "$enable_sse5" = "yes" ; then
        COMP_VENDOR_CFLAGS="-msse5 $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_sse4a" = "yes" ; then
        COMP_VENDOR_CFLAGS="-msse4a $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_sse4_2" = "yes" ; then
        COMP_VENDOR_CFLAGS="-msse42 $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_sse4_1" = "yes" ; then
        COMP_VENDOR_CFLAGS="-msse41 $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_sse3" = "yes" ; then
        COMP_VENDOR_CFLAGS="-msse3 $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_sse2" = "yes" ; then
        COMP_VENDOR_CFLAGS="-msse2 $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_sse" = "yes" ; then
        COMP_VENDOR_CFLAGS="-msse $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_mmx" = "yes" ; then
        COMP_VENDOR_CFLAGS="-mmmx $COMP_VENDOR_CFLAGS"
    fi
    case $host_os in
    mingw* | cygwin*)
        COMP_VENDOR_LDFLAGS="-no-undefined"
        ;;
    *)
        COMP_VENDOR_LDFLAGS=
        ;;
    esac
    ;;
sun)
    COMP_VENDOR_CFLAGS="-xc99=all -mt -xCC -errwarn=%all -xvpara"
    if test "$enable_sse3" = "yes" ; then
        COMP_VENDOR_CFLAGS="-native -fast $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_sse2" = "yes" ; then
        COMP_VENDOR_CFLAGS="-native -fast $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_sse" = "yes" ; then
        COMP_VENDOR_CFLAGS="-native -fast $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_mmx" = "yes" ; then
        COMP_VENDOR_CFLAGS="-native -fast $COMP_VENDOR_CFLAGS"
    fi
    COMP_VENDOR_LDFLAGS=
    ;;
*)
    COMP_VENDOR_CFLAGS="-std=c99 -Wall -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes"
    COMP_VENDOR_LDFLAGS=
    ;;
esac

COMP_VENDOR_CFLAGS="-DNDEBUG $COMP_VENDOR_CFLAGS"

AM_CONDITIONAL([COND_DOC], [test "$enable_doc" = yes])
AM_CONDITIONAL([COND_TESTS], [test "$enable_tests" = yes])
AM_CONDITIONAL([COND_TESTDATA], [test "$enable_test_data" = yes])
AM_CONDITIONAL([COND_MMX], [test "$enable_mmx" = yes])
AM_CONDITIONAL([COND_SSE], [test "$enable_sse" = yes])
AM_CONDITIONAL([COND_SSE2], [test "$enable_sse2" = yes])
AM_CONDITIONAL([COND_SSE3], [test "$enable_sse3" = yes])
AM_CONDITIONAL([COND_SSE4_1], [test "$enable_sse4_1" = yes])
AM_CONDITIONAL([COND_SSE4_2], [test "$enable_sse4_2" = yes])
AM_CONDITIONAL([COND_SSE4A], [test "$enable_sse4a" = yes])
AM_CONDITIONAL([COND_SSE5], [test "$enable_sse5" = yes])
if test "$enable_fixed_point" = "yes" ; then
    AC_DEFINE([G722_1_USE_FIXED_POINT], [1], [Enable fixed point processing, where possible, instead of floating point])
    G722_1_USE_FIXED_POINT="#define G722_1_USE_FIXED_POINT 1"
    fixed = "yes"
    G722_1_VECTORS_FOR_TESTS="fixed"
else
    AX_FIXED_POINT_MACHINE([$host],
        [
	AC_DEFINE([G722_1_USE_FIXED_POINT], [1], [Enable fixed point processing, where possible, instead of floating point])
        G722_1_USE_FIXED_POINT="#define G722_1_USE_FIXED_POINT 1"
	fixed = "yes"
	],
        [G722_1_USE_FIXED_POINT="#undef G722_1_USE_FIXED_POINT"])
    G722_1_VECTORS_FOR_TESTS="floating"
fi
AM_CONDITIONAL([COND_FIXED], [test "$fixed" = "yes"])

AX_MISALIGNED_ACCESS_FAILS([$host],
    [AC_DEFINE([G722_1_MISALIGNED_ACCESS_FAILS], [1], [Do not expect a misaligned memory access to work correctly])
    G722_1_MISALIGNED_ACCESS_FAILS="#define G722_1_MISALIGNED_ACCESS_FAILS 1"],
    [G722_1_MISALIGNED_ACCESS_FAILS="#undef G722_1_MISALIGNED_ACCESS_FAILS"])

if test "$enable_sse5" = "yes" ; then
    AC_DEFINE([SPANDSP_USE_SSE5], [1], [Use the SSE5 instruction set (i386 and x86_64 only).])
    enable_sse4a="yes"
fi
if test "$enable_sse4a" = "yes" ; then
    AC_DEFINE([SPANDSP_USE_SSE4A], [1], [Use the SSE4A instruction set (i386 and x86_64 only).])
    enable_sse4_2="yes"
fi
if test "$enable_sse4_2" = "yes" ; then
    AC_DEFINE([SPANDSP_USE_SSE4_2], [1], [Use the SSE4.2 instruction set (i386 and x86_64 only).])
    enable_sse4_1="yes"
fi
if test "$enable_sse4_1" = "yes" ; then
    AC_DEFINE([SPANDSP_USE_SSE4_1], [1], [Use the SSE4.1 instruction set (i386 and x86_64 only).])
    enable_sse3="yes"
fi
if test "$enable_sse3" = "yes" ; then
    AC_DEFINE([SPANDSP_USE_SSE3], [1], [Use the SSE3 instruction set (i386 and x86_64 only).])
    enable_sse2="yes"
fi
if test "$enable_sse2" = "yes" ; then
    AC_DEFINE([SPANDSP_USE_SSE2], [1], [Use the SSE2 instruction set (i386 and x86_64 only).])
    enable_sse="yes"
fi
if test "$enable_sse" = "yes" ; then
    AC_DEFINE([SPANDSP_USE_SSE], [1], [Use the SSE instruction set (i386 and x86_64 only).])
    enable_mmx="yes"
fi
if test "$enable_mmx" = "yes" ; then
    AC_DEFINE([SPANDSP_USE_MMX], [1], [Use the MMX instruction set (i386 and x86_64 only).])
fi

AC_SUBST(CC_FOR_BUILD)
AC_SUBST(COMP_VENDOR_CFLAGS)
AC_SUBST(COMP_VENDOR_LDFLAGS)
AC_SUBST(TESTLIBS)
AC_SUBST(G722_1_USE_FIXED_POINT)
AC_SUBST(G722_1_VECTORS_FOR_TESTS)
AC_SUBST(INSERT_INTTYPES_HEADER)
AC_SUBST(INSERT_STDINT_HEADER)
AC_SUBST(INSERT_TGMATH_HEADER)
AC_SUBST(INSERT_MATH_HEADER)

AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 test-data/Makefile
                 test-data/local/Makefile
                 test-data/itu/Makefile
                 src/Makefile
                 src/g722_1.h
                 tests/Makefile
                 g722_1.spec])

AC_CONFIG_FILES([tests/regression_tests.sh], [chmod +x tests/regression_tests.sh])

AC_OUTPUT

# @end 1

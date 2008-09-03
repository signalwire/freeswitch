#
# SpanDSP - a series of DSP components for telephony
#
# configure.ac - Process this file with autoconf to produce configure
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License version 2.1,
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $Id: configure.ac,v 1.56 2008/07/10 12:44:54 steveu Exp $

# @start 1

AC_INIT

m4_include(config/ax_compiler_vendor.m4)
m4_include(config/ax_check_real_file.m4)
m4_include(config/ax_fixed_point_machine.m4)
m4_include(config/ax_misaligned_access_fails.m4)
m4_include(config/ax_c99_features.m4)

SPANDSP_MAJOR_VERSION=0
SPANDSP_MINOR_VERSION=0
SPANDSP_MICRO_VERSION=5

SPANDSP_LT_CURRENT=1
SPANDSP_LT_REVISION=0
SPANDSP_LT_AGE=0

VERSION=$SPANDSP_MAJOR_VERSION.$SPANDSP_MINOR_VERSION.$SPANDSP_MICRO_VERSION
PACKAGE=spandsp

AC_SUBST(SPANDSP_LT_CURRENT)
AC_SUBST(SPANDSP_LT_REVISION)
AC_SUBST(SPANDSP_LT_AGE)

AC_CONFIG_SRCDIR([src/tone_generate.c])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADERS([src/config.h:config-h.in])
AM_INIT_AUTOMAKE($PACKAGE, $VERSION)

AC_CANONICAL_HOST
AC_CANONICAL_BUILD
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
AC_ARG_ENABLE(test_data,    [  --enable-test-data   Build TIFF test files for some ITU test images])
AC_ARG_ENABLE(mmx,          [  --enable-mmx         Enable MMX support])
AC_ARG_ENABLE(sse,          [  --enable-sse         Enable SSE support])
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
AC_CHECK_HEADERS([fftw3.h], , [AC_CHECK_HEADERS([fftw.h])])
AC_CHECK_HEADERS([tiffio.h])
AC_CHECK_HEADERS([pthread.h])
AC_CHECK_HEADERS([unicall.h])
if test "${build}" == "${host}"
then
  AC_CHECK_HEADERS([X11/X.h])
fi

# Determine XML2 include path
AC_MSG_CHECKING(for libxml/xmlmemory.h)

# Can we include headers using system include dirs?
AC_TRY_COMPILE([#include <libxml/xmlmemory.h>], [int a = 1;],
    XML2_INCLUDE=" ",
    XML2_INCLUDE=
)

# Hunt through several possible directories to find the includes for libxml2
if test "x$XML2_INCLUDE" = "x"; then
    old_CPPFLAGS="$CPPFLAGS"
    for i in $xml2_include_dir /usr/include /usr/local/include /usr/include/libxml2 /usr/local/include/libxml2 ; do
        CPPFLAGS="$old_CPPFLAGS -I$i"
        AC_TRY_COMPILE([#include <libxml/xmlmemory.h>], [int a = 1;],
            XML2_INCLUDE="-I$i",
            XML2_INCLUDE=
        )
        if test "x$XML2_INCLUDE" != "x"; then
            break;
        fi
    done
    CPPFLAGS="$old_CPPFLAGS $XML2_INCLUDE"
fi

AC_CHECK_HEADERS([libxml/xmlmemory.h])
AC_CHECK_HEADERS([libxml/parser.h])
AC_CHECK_HEADERS([libxml/xinclude.h])

AC_LANG([C++])
AC_CHECK_HEADERS([FL/Fl.H])
AC_CHECK_HEADERS([FL/Fl_Overlay_Window.H])
AC_CHECK_HEADERS([FL/Fl_Light_Button.H])
AC_CHECK_HEADERS([FL/fl_draw.H])
AC_CHECK_HEADERS([FL/Fl_Cartesian.H], [], [], [],[[#include <FL/Fl.H>
]])
AC_CHECK_HEADERS([FL/Fl_Audio_Meter.H], [], [], [],[[#include <FL/Fl.H>
]])

AC_LANG([C])

if test "${build}" == "${host}"
then
    case "${host}" in
    x86_64-*)
        AX_CHECK_REAL_FILE([${prefix}/lib64], libdir='$(exec_prefix)/lib64')
        ;;
    esac
fi

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

if test "${build}" == "${host}"
then
    case "${host}" in
    x86_64-*)
        # X86_64 Linux machines may have both 64 bit and 32 bit libraries. We need to choose the right set
        AX_CHECK_REAL_FILE([/usr/X11R6/lib64], [TESTLIBS="$TESTLIBS -L/usr/X11R6/lib64"], AC_CHECK_FILE([/usr/X11R6/lib], [TESTLIBS="$TESTLIBS -L/usr/X11R6/lib"]))
        ;;
    esac
fi

# Checks for libraries.
AC_CHECK_LIB([tiff], [TIFFOpen], , AC_MSG_ERROR("Can't build without libtiff (does your system require a libtiff-devel package?)"), -lm)

AC_CHECK_LIB([xml2], [xmlParseFile], [AC_DEFINE([HAVE_LIBXML2], [1], [Define to 1 if you have the 'libxml2' library (-lxml2).]) TESTLIBS="$TESTLIBS -lxml2"])

if test -n "$enable_tests" ; then
    AC_LANG([C])
    AC_CHECK_LIB([audiofile], [afOpenFile], TESTLIBS="$TESTLIBS -laudiofile", AC_MSG_ERROR("Can't make tests without libaudiofile (does your system require a libaudiofile-devel package?)"))
    AC_CHECK_LIB([fftw3], [fftw_plan_dft_1d], TESTLIBS="$TESTLIBS -lfftw3", [AC_CHECK_LIB([fftw], [fftw_create_plan], TESTLIBS="$TESTLIBS -lfftw")])
    AC_CHECK_LIB([pthread], [pthread_attr_init], TESTLIBS="$TESTLIBS -lpthread")
    AC_CHECK_LIB([dl], [dlopen], TESTLIBS="$TESTLIBS -ldl")
    AC_CHECK_LIB([unicall], [uc_start], TESTLIBS="$TESTLIBS -lunicall",, -ltiff -ldl)
    AC_CHECK_LIB([Xft], [XftFontOpen], TESTLIBS="$TESTLIBS -lXft",, $TESTLIBS)
    AC_CHECK_LIB([Xext], [XextCreateExtension], TESTLIBS="$TESTLIBS -lXext",, $TESTLIBS)
    AC_CHECK_LIB([X11], [XOpenDisplay], TESTLIBS="$TESTLIBS -lX11",, $TESTLIBS)
    AC_LANG([C++])
    AC_CHECK_LIB([fltk], [main], TESTLIBS="$TESTLIBS -lfltk -lsupc++",, $TESTLIBS)
    AC_CHECK_LIB([fltk_cartesian], [main], TESTLIBS="-lfltk_cartesian $TESTLIBS",, $TESTLIBS)
    AC_CHECK_LIB([fltk_audio_meter], [main], TESTLIBS="-lfltk_audio_meter $TESTLIBS",, $TESTLIBS)
    AC_LANG([C])
fi

case "${ax_cv_c_compiler_vendor}" in
gnu)
    COMP_VENDOR_CFLAGS="-std=gnu99 -ffast-math -Wall -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes"
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
if test "$enable_fixed_point" = "yes" ; then
    AC_DEFINE([SPANDSP_USE_FIXED_POINT], [1], [Enable fixed point processing, where possible, instead of floating point])
    SPANDSP_USE_FIXED_POINT="#define SPANDSP_USE_FIXED_POINT 1"
else
    AX_FIXED_POINT_MACHINE([$host],
        [AC_DEFINE([SPANDSP_USE_FIXED_POINT], [1], [Enable fixed point processing, where possible, instead of floating point])
        SPANDSP_USE_FIXED_POINT="#define SPANDSP_USE_FIXED_POINT 1"],
        [SPANDSP_USE_FIXED_POINT="#undef SPANDSP_USE_FIXED_POINT"])
fi
AX_MISALIGNED_ACCESS_FAILS([$host],
    [AC_DEFINE([SPANDSP_MISALIGNED_ACCESS_FAILS], [1], [Do not expect a misaligned memory access to work correctly])
    SPANDSP_MISALIGNED_ACCESS_FAILS="#define SPANDSP_MISALIGNED_ACCESS_FAILS 1"],
    [SPANDSP_MISALIGNED_ACCESS_FAILS="#undef SPANDSP_MISALIGNED_ACCESS_FAILS"])

AC_SUBST(CC_FOR_BUILD)
AC_SUBST(CPPFLAGS_FOR_BUILD)
AC_SUBST(COMP_VENDOR_CFLAGS)
AC_SUBST(COMP_VENDOR_LDFLAGS)
AC_SUBST(TESTLIBS)
AC_SUBST(SPANDSP_USE_FIXED_POINT)
AC_SUBST(SPANDSP_MISALIGNED_ACCESS_FAILS)
AC_SUBST(INSERT_INTTYPES_HEADER)
AC_SUBST(INSERT_STDINT_HEADER)
AC_SUBST(INSERT_TGMATH_HEADER)
AC_SUBST(INSERT_MATH_HEADER)

AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 doc/doxygen
                 src/Makefile
                 src/spandsp.h
                 spandsp-sim/Makefile
                 test-data/Makefile
                 test-data/etsi/Makefile
                 test-data/etsi/fax/Makefile
                 test-data/itu/Makefile
                 test-data/itu/fax/Makefile
                 test-data/local/Makefile
                 tests/Makefile
                 spandsp.spec])

AC_OUTPUT

# @end 1

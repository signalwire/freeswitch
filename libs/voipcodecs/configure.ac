#
# VoIPcodecs - a series of DSP components for telephony
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
# $Id: configure.ac,v 1.38 2007/11/10 11:14:56 steveu Exp $

# @start 1

AC_INIT

AC_DEFUN([AX_COMPILER_VENDOR],
[
AC_CACHE_CHECK([for _AC_LANG compiler vendor], ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor,
 [ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor=unknown
  # note: don't check for GCC first, since some other compilers define __GNUC__
  for ventest in intel:__ICC,__ECC,__INTEL_COMPILER ibm:__xlc__,__xlC__,__IBMC__,__IBMCPP__ gnu:__GNUC__ sun:__SUNPRO_C,__SUNPRO_CC hp:__HP_cc,__HP_aCC dec:__DECC,__DECCXX,__DECC_VER,__DECCXX_VER borland:__BORLANDC__,__TURBOC__ comeau:__COMO__ cray:_CRAYC kai:__KCC lcc:__LCC__ metrowerks:__MWERKS__ sgi:__sgi,sgi microsoft:_MSC_VER watcom:__WATCOMC__ portland:__PGI; do
    vencpp="defined("`echo $ventest | cut -d: -f2 | sed 's/,/) || defined(/g'`")"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[
#if !($vencpp)
      thisisanerror;
#endif
])], [ax_cv_]_AC_LANG_ABBREV[_compiler_vendor=`echo $ventest | cut -d: -f1`; break])
  done
 ])
])

VOIPCODECS_MAJOR_VERSION=0
VOIPCODECS_MINOR_VERSION=0
VOIPCODECS_MICRO_VERSION=1

VOIPCODECS_LT_CURRENT=0
VOIPCODECS_LT_REVISION=2
VOIPCODECS_LT_AGE=0

VERSION=$VOIPCODECS_MAJOR_VERSION.$VOIPCODECS_MINOR_VERSION.$VOIPCODECS_MICRO_VERSION
PACKAGE=voipcodecs

AC_SUBST(VOIPCODECS_LT_CURRENT)
AC_SUBST(VOIPCODECS_LT_REVISION)
AC_SUBST(VOIPCODECS_LT_AGE)

AC_CONFIG_SRCDIR([src/g711.c])
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

  CC=${CC-${host_alias}-gcc}
  CFLAGS=${CFLAGS-"-g -O2"}
  CXX=${CXX-${host_alias}-c++}
  CXXFLAGS=${CXXFLAGS-"-g -O2"}
  CC_FOR_BUILD=${CC_FOR_BUILD-gcc}
else
  # Set reasonable default values for some tools even if not Canadian.
  # Of course, these are different reasonable default values, originally
  # specified directly in the Makefile.
  # We don't export, so that autoconf can do its job.
  # Note that all these settings are above the fragment inclusion point
  # in Makefile.in, so can still be overridden by fragments.
  # This is all going to change when we autoconfiscate...
  CC_FOR_BUILD="\$(CC)"
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
AC_ARG_ENABLE(itutests,     [  --enable-itutests    Build TIFF test files for some ITU test images])
AC_ARG_ENABLE(mmx,          [  --enable-mmx         Enable MMX support])
AC_ARG_ENABLE(sse,          [  --enable-sse         Enable SSE support])
AC_ARG_ENABLE(fixed_point,  [  --enable-fixed-point Enable fixed point support])

AC_FUNC_ERROR_AT_LINE
AC_FUNC_VPRINTF
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES

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
AC_CHECK_HEADERS([fftw.h])
AC_CHECK_HEADERS([fftw3.h])
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

if test "${build}" == "${host}"
then
    case "${host}" in
    x86_64-*)
        AC_CHECK_FILE([${prefix}/lib64], libdir='$(exec_prefix)/lib64')
        ;;
    esac
fi

AC_LANG([C])

if test "${build}" == "${host}"
then
    case "${host}" in
    x86_64-*)
        # X86_64 Linux machines may have both 64 bit and 32 bit libraries. We need to choose the right set
        AC_CHECK_FILE([/usr/X11R6/lib64], [TESTLIBS="$TESTLIBS -L/usr/X11R6/lib64"], AC_CHECK_FILE([/usr/X11R6/lib], [TESTLIBS="$TESTLIBS -L/usr/X11R6/lib"]))
        ;;
    esac
fi

# Checks for libraries.
AC_CHECK_LIB([m], [cos])
AC_CHECK_LIB([m], [pow])
AC_CHECK_LIB([m], [sqrt])
if test -n "$enable_tests" ; then
    AC_LANG([C++])
    AC_CHECK_LIB([fltk], [main], TESTLIBS="$TESTLIBS -lfltk -lsupc++")
    AC_CHECK_LIB([fltk_cartesian], [main], TESTLIBS="-lfltk_cartesian $TESTLIBS")
    AC_CHECK_LIB([fltk_audio_meter], [main], TESTLIBS="-lfltk_audio_meter $TESTLIBS")
    AC_LANG([C])
    AC_CHECK_LIB([audiofile], [afOpenFile], TESTLIBS="$TESTLIBS -laudiofile", AC_MSG_ERROR("Can't make tests without libaudiofile (does your system require a libaudiofile-devel package?)"))
    AC_CHECK_LIB([fftw3], [fftw_plan_dft_1d], TESTLIBS="$TESTLIBS -lfftw3")
    AC_CHECK_LIB([fftw], [fftw_create_plan], TESTLIBS="$TESTLIBS -lfftw")
    AC_CHECK_LIB([pthread], [pthread_attr_init], TESTLIBS="$TESTLIBS -lpthread")
    AC_CHECK_LIB([dl], [dlopen], TESTLIBS="$TESTLIBS -ldl")
    AC_CHECK_LIB([unicall], [uc_start], TESTLIBS="$TESTLIBS -lunicall",, -ltiff -ldl)
    AC_CHECK_LIB([Xft], [XftFontOpen], TESTLIBS="$TESTLIBS -lXft",, $TESTLIBS)
    AC_CHECK_LIB([Xext], [XextCreateExtension], TESTLIBS="$TESTLIBS -lXext",, $TESTLIBS)
    AC_CHECK_LIB([X11], [XOpenDisplay], TESTLIBS="$TESTLIBS -lX11",, $TESTLIBS)
    AC_CHECK_LIB([xml2], [xmlParseFile], [AC_DEFINE([HAVE_LIBXML2], [1], [Define to 1 if you have the 'libxml2' library (-lxml2).]) TESTLIBS="$TESTLIBS -lxml2"])
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
    ;;
sun)
    COMP_VENDOR_CFLAGS="-xc99=all -mt -xCC -errwarn=%all -xvpara"
    if test "$enable_sse" = "yes" ; then
        COMP_VENDOR_CFLAGS="-native -fast $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_mmx" = "yes" ; then
        COMP_VENDOR_CFLAGS="-native -fast $COMP_VENDOR_CFLAGS"
    fi
    ;;
*)
    COMP_VENDOR_CFLAGS="-std=c99 -Wall -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes"
    ;;
esac

COMP_VENDOR_CFLAGS="-DNDEBUG $COMP_VENDOR_CFLAGS"

AM_CONDITIONAL([COND_DOC], [test "$enable_doc" = yes])
AM_CONDITIONAL([COND_TESTS], [test "$enable_tests" = yes])
AM_CONDITIONAL([COND_ITUTESTS], [test "$enable_itutests" = yes])
AM_CONDITIONAL([COND_MMX], [test "$enable_mmx" = yes])
AM_CONDITIONAL([COND_SSE], [test "$enable_sse" = yes])
if test "$enable_fixed_point" = "yes" ; then
    AC_DEFINE([VOIPCODECS_USE_FIXED_POINT], [1], [Enable fixed point processing, where possible, instead of floating point])
    VOIPCODECS_USE_FIXED_POINT="#define VOIPCODECS_USE_FIXED_POINT 1"
else
#
# So far we deal with the embedded ARM, Blackfin, MIPS, TI DSP and XScale processors as
# things which lack fast hardware floating point.
#
# Other candidates would be the small embedded Power PCs.
#
    case $basic_machine in
	      arc | arm | arm[bl]e | arme[lb] | armv[2345] | armv[345][lb] \
	    | bfin \
	    | mips | mipsbe | mipseb | mipsel | mipsle \
	    | tic54x | c54x* | tic55x | c55x* | tic6x | c6x* \
	    | xscale | xscalee[bl] \
	    | arm-*  | armbe-* | armle-* | armeb-* | armv*-* \
	    | bfin-* \
	    | mips-* | mipsbe-* | mipseb-* | mipsel-* | mipsle-* \
	    | tic30-* | tic4x-* | tic54x-* | tic55x-* | tic6x-* | tic80-* \
	    | xscale-* | xscalee[bl]-* )
        AC_DEFINE([VOIPCODECS_USE_FIXED_POINT], [1], [Enable fixed point processing, where possible, instead of floating point])
        VOIPCODECS_USE_FIXED_POINT="#define VOIPCODECS_USE_FIXED_POINT 1"
        ;;
    *)
        VOIPCODECS_USE_FIXED_POINT="#undef VOIPCODECS_USE_FIXED_POINT"
        ;;
    esac
fi

AC_SUBST(CC_FOR_BUILD)
AC_SUBST(COMP_VENDOR_CFLAGS)
AC_SUBST(TESTLIBS)
AC_SUBST(VOIPCODECS_USE_FIXED_POINT)
AC_SUBST(INSERT_INTTYPES_HEADER)
AC_SUBST(INSERT_STDINT_HEADER)
AC_SUBST(INSERT_TGMATH_HEADER)
AC_SUBST(INSERT_MATH_HEADER)

AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 etsitests/Makefile
                 itutests/Makefile
                 localtests/Makefile
                 src/Makefile
                 src/voipcodecs.h
                 tests/Makefile
                 voipcodecs.spec])

AC_OUTPUT

# @end 1

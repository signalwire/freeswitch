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

# @start 1

AC_PREREQ([2.59])
AC_INIT([spandsp], [1.99.0])

CFLAGS="$CFLAGS $CONFIGURE_CFLAGS"
CXXFLAGS="$CXXFLAGS $CONFIGURE_CXXFLAGS"
LDFLAGS="$LDFLAGS $CONFIGURE_LDFLAGS"

SPANDSP_LT_CURRENT=3
SPANDSP_LT_REVISION=0
SPANDSP_LT_AGE=0

m4_include(m4/ax_compiler_vendor.m4)
m4_include(m4/ax_check_real_file.m4)
m4_include(m4/ax_fixed_point_machine.m4)
m4_include(m4/ax_misaligned_access_fails.m4)
m4_include(m4/ax_c99_features.m4)
m4_include(m4/ax_check_export_capability.m4)
m4_include(m4/ax_check_arm_neon.m4)
m4_include(m4/ax_func_aligned_alloc.m4)

AC_CONFIG_SRCDIR([src/tone_generate.c])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([src/config.h:config-h.in])
AM_INIT_AUTOMAKE([1.9.5])

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

AC_DEFUN([REMOVE_FROM_VAR],[
    new_val=""
    removed=0
    for i in $$1; do
        if test "x$i" != "x$2"; then
            new_val="$new_val $i"
        else
            removed=1
        fi
    done
    if test $removed = "1"; then
        echo "  removed \"$2\" from $1"
        $1=$new_val
    fi
])

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
AC_ARG_ENABLE(ssse3,        [  --enable-ssse3       Enable SSSE3 support])
AC_ARG_ENABLE(sse4_1,       [  --enable-sse4-1      Enable SSE4.1 support])
AC_ARG_ENABLE(sse4_2,       [  --enable-sse4-2      Enable SSE4.2 support])
AC_ARG_ENABLE(avx,          [  --enable-avx         Enable AVX support])
AC_ARG_ENABLE(avx2,         [  --enable-avx2        Enable AVX2 support])
AC_ARG_ENABLE(neon,         [  --enable-neon        Enable NEON support])
AC_ARG_ENABLE(fixed_point,  [  --enable-fixed-point Enable fixed point support])
AC_ARG_ENABLE(v32bis,       [  --enable-v32bis      Enable V.32bis support])
AC_ARG_ENABLE(v34,          [  --enable-v34         Enable V.34 support])

# The following is for MSVC, where we may be using a local copy of libtiff, built alongside spandsp
AC_ARG_ENABLE(builtin_tiff,
    [AC_HELP_STRING([--enable-builtin-tiff],[build with builtin libtiff])],[enable_builtin_tiff="$enableval"],[enable_builtin_tiff="no"])

AC_FUNC_ERROR_AT_LINE
AC_FUNC_VPRINTF
AC_FUNC_MEMCMP
AC_FUNC_MEMMOVE
if test "${ax_cv_c_compiler_vendor}" = "gnu"
  then
    AC_FUNC_REALLOC
    AC_FUNC_MALLOC
fi
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

AX_FUNC_ALIGNED_ALLOC
AC_CHECK_FUNCS([memalign])
AC_CHECK_FUNCS([posix_memalign])
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
AC_CHECK_FUNCS([drand48])

AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_TIME

# Check for header files.
AC_CHECK_HEADERS([socket.h])
AC_CHECK_HEADERS([inttypes.h], [INSERT_INTTYPES_HEADER="#include <inttypes.h>"])
AC_CHECK_HEADERS([stdint.h], [INSERT_STDINT_HEADER="#include <stdint.h>"])
AC_CHECK_HEADERS([stdatomic.h])
AC_CHECK_HEADERS([stdbool.h], [INSERT_STDBOOL_HEADER="#include <stdbool.h>"], [INSERT_STDBOOL_HEADER="#include <spandsp/stdbool.h>"])
AC_CHECK_HEADERS([stdfix.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([strings.h])
AC_CHECK_HEADERS([malloc.h])
AC_CHECK_HEADERS([math.h], [INSERT_MATH_HEADER="#include <math.h>"])
AC_CHECK_HEADERS([float.h])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([sys/select.h])
AC_CHECK_HEADERS([sys/ioctl.h])
AC_CHECK_HEADERS([sys/fcntl.h])
AC_CHECK_HEADERS([sndfile.h])
AC_CHECK_HEADERS([fenv.h])
AC_CHECK_HEADERS([fftw3.h], , [AC_CHECK_HEADERS([fftw.h])])
AC_CHECK_HEADERS([pcap.h])
AC_CHECK_HEADERS([pthread.h])

case "$host" in
    *dragonfly*)
    ;;
    *freebsd*)
    ;;
    *netbsd*)
    ;;
    *)
    AC_CHECK_HEADERS([tgmath.h], [INSERT_TGMATH_HEADER="#include <tgmath.h>"])
    ;;
esac

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
AC_CHECK_HEADERS([FL/Fl_Cartesian.H])
AC_CHECK_HEADERS([FL/Fl_Audio_Meter.H])

AC_LANG([C])

if test "${build}" == "${host}"
then
    case "${host}" in
    x86_64-*)
        # X86_64 Linux machines may have both 64 bit and 32 bit libraries. We need to choose the right set
        AX_CHECK_REAL_FILE([${prefix}/lib64], libdir='${exec_prefix}/lib64')
        AX_CHECK_REAL_FILE([/usr/X11R6/lib64], [TESTLIBS="$TESTLIBS -L/usr/X11R6/lib64"], AC_CHECK_FILE([/usr/X11R6/lib], [TESTLIBS="$TESTLIBS -L/usr/X11R6/lib"]))
        # The very oldest AMD 64 bit chips support SSE2, SSE and MMX
        enable_sse2="yes"
        ;;
    esac
fi

AC_CHECK_LIB([m], [cos])
# Some platforms still seem to lack the basic single precision trig and power related functions.
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

AC_SEARCH_LIBS([open_memstream], [m], AC_DEFINE([HAVE_OPEN_MEMSTREAM], [1], [Define to 1 if you have the open_memstream() function.]))

if test -n "$enable_tests" ; then
    AC_CHECK_PROG([HAVE_SOX], [sox], yes)
    if test "x$HAVE_SOX" != "xyes" ; then
        AC_MSG_ERROR("Cannot make tests without sox installed")
    fi
    AC_CHECK_PROG([HAVE_PBMTOG3], [pbmtog3], yes)
    if test "x$HAVE_PBMTOG3" != "xyes" ; then
        AC_MSG_ERROR("Cannot make tests without pbmtog3 installed (does your system require a netpbm-progs package?)")
    fi
    AC_CHECK_PROG([HAVE_FAX2TIFF], [fax2tiff], yes)
    if test "x$HAVE_FAX2TIFF" != "xyes" ; then
        AC_MSG_ERROR("Cannot make tests without fax2tiff installed (does your system require a libtiff-tools package?)")
    fi
    AC_LANG([C])
    # Checks for libraries.
    AC_CHECK_LIB([sndfile], [sf_open], SIMLIBS="$SIMLIBS -lsndfile", AC_MSG_ERROR("Cannot make tests without libsndfile (does your system require a libsndfile-devel package?)"))
    AC_CHECK_LIB([fftw3], [fftw_plan_dft_1d], SIMLIBS="$SIMLIBS -lfftw3", [AC_CHECK_LIB([fftw], [fftw_create_plan], SIMLIBS="$SIMLIBS -lfftw", AC_MSG_ERROR("Cannot make tests without FFTW 2 or 3 (does your system require an fftw?-devel package?)"))])
    AC_CHECK_LIB([xml2], [xmlParseFile], TESTLIBS="$TESTLIBS -lxml2", AC_MSG_ERROR("Cannot make tests without libxml2 (does your system require a libxml2-devel package?)"))
    AC_CHECK_LIB([pcap], [pcap_open_offline], TESTLIBS="$TESTLIBS -lpcap", [AC_CHECK_LIB([wpcap], [pcap_open_offline], TESTLIBS="$TESTLIBS -lwpcap",  AC_MSG_ERROR("Cannot make tests without libpcap (does your system require an libpcap-devel package?)"))])
    AC_CHECK_LIB([pthread], [pthread_attr_init], TESTLIBS="$TESTLIBS -lpthread")
    AC_CHECK_LIB([dl], [dlopen], TESTLIBS="$TESTLIBS -ldl")
    AC_CHECK_LIB([Xft], [XftFontOpen], TESTLIBS="$TESTLIBS -lXft",, $TESTLIBS)
    AC_CHECK_LIB([Xext], [XextCreateExtension], TESTLIBS="$TESTLIBS -lXext",, $TESTLIBS)
    AC_CHECK_LIB([X11], [XOpenDisplay], TESTLIBS="$TESTLIBS -lX11",, $TESTLIBS)
    AC_LANG([C++])
    AC_CHECK_LIB([fltk], [main], TESTLIBS="$TESTLIBS -lfltk -lsupc++",, $TESTLIBS)
    AC_CHECK_LIB([fltk_cartesian], [main], TESTLIBS="-lfltk_cartesian $TESTLIBS",, $TESTLIBS)
    AC_CHECK_LIB([fltk_audio_meter], [main], TESTLIBS="-lfltk_audio_meter $TESTLIBS",, $TESTLIBS)
    AC_LANG([C])
fi

AX_CHECK_EXPORT_CAPABILITY([$host],
    [AC_DEFINE([SPANDSP_USE_EXPORT_CAPABILITY], [1], [Use the library symbol export capability of the compiler])
    SPANDSP_USE_EXPORT_CAPABILITY="#define SPANDSP_USE_EXPORT_CAPABILITY 1"],
    [SPANDSP_USE_EXPORT_CAPABILITY="#undef SPANDSP_USE_EXPORT_CAPABILITY"])

saved_CFLAGS="$CFLAGS"
AC_CACHE_CHECK([whether compiler supports -Wunused-but-set-variable], [ac_cv_gcc_unused_but_set_variable], [
    # We need to add -Werror here or clang doesn't fail (it just warns), even though it doesn't understand the
    # -Wunused-but-set-variable tag
    CFLAGS="$CFLAGS -Werror -Wunused-but-set-variable"
    AC_TRY_COMPILE([],[return 0;],[ac_cv_gcc_unused_but_set_variable=yes],[ac_cv_gcc_unused_but_set_variable=no])
    ])
AC_MSG_RESULT($ac_cv_gcc_unused_but_set_variable)
CFLAGS="$saved_CFLAGS"

case "${ax_cv_c_compiler_vendor}" in
gnu)
    COMP_VENDOR_CFLAGS="-std=gnu99 -ffast-math -Wall -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes $COMP_VENDOR_CFLAGS"
    if test x"$ac_cv_gcc_unused_but_set_variable" = xyes ; then
        COMP_VENDOR_CFLAGS="-Wunused-but-set-variable $COMP_VENDOR_CFLAGS"
    fi
    AX_CHECK_ARM_NEON([$host],
        [AC_DEFINE([SPANDSP_USE_ARM_NEON], [1], [Use the ARM NEON instruction set])])
    if test "$enable_neon" = "yes" ; then
        COMP_VENDOR_CFLAGS="-mfpu=neon $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_avx2" = "yes" ; then
        COMP_VENDOR_CFLAGS="-mavx2 $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_avx" = "yes" ; then
        COMP_VENDOR_CFLAGS="-mavx $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_sse4_2" = "yes" ; then
        COMP_VENDOR_CFLAGS="-msse42 $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_sse4_1" = "yes" ; then
        COMP_VENDOR_CFLAGS="-msse41 $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_ssse3" = "yes" ; then
        COMP_VENDOR_CFLAGS="-mssse3 $COMP_VENDOR_CFLAGS"
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
    cygwin*)
        COMP_VENDOR_LDFLAGS="-no-undefined"
        ;;
    mingw*)
        COMP_VENDOR_LDFLAGS="-no-undefined -lws2_32"
        ;;
    *)
        COMP_VENDOR_LDFLAGS=
        ;;
    esac
    ;;
sun)
    COMP_VENDOR_CFLAGS="-xc99=all -mt -xCC -errwarn=%all -xvpara $COMP_VENDOR_CFLAGS"
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
    REMOVE_FROM_VAR(CFLAGS, -Xc)
    ;;
intel)
    COMP_VENDOR_CFLAGS="-std=c99 -D_POSIX_C_SOURCE=2 -D_GNU_SOURCE=1 -Wall -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes $COMP_VENDOR_CFLAGS"
    if test x"$ac_cv_gcc_unused_but_set_variable" = xyes ; then
        COMP_VENDOR_CFLAGS="-Wunused-but-set-variable $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_avx2" = "yes" ; then
        COMP_VENDOR_CFLAGS="-mavx2 $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_avx" = "yes" ; then
        COMP_VENDOR_CFLAGS="-mavx $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_sse4_2" = "yes" ; then
        COMP_VENDOR_CFLAGS="-msse42 $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_sse4_1" = "yes" ; then
        COMP_VENDOR_CFLAGS="-msse41 $COMP_VENDOR_CFLAGS"
    fi
    if test "$enable_ssse3" = "yes" ; then
        COMP_VENDOR_CFLAGS="-mssse3 $COMP_VENDOR_CFLAGS"
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
    COMP_VENDOR_LDFLAGS=
    ;;
clang*)
    COMP_VENDOR_CFLAGS="-D_XOPEN_SOURCE=700 -std=c99 -Wall -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes $COMP_VENDOR_CFLAGS"
    if test x"$ac_cv_gcc_unused_but_set_variable" = xyes ; then
        COMP_VENDOR_CFLAGS="-Wunused-but-set-variable $COMP_VENDOR_CFLAGS"
    fi
    COMP_VENDOR_LDFLAGS=
    ;;
*)
    COMP_VENDOR_CFLAGS="-std=c99 -Wall -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes $COMP_VENDOR_CFLAGS"
    if test x"$ac_cv_gcc_unused_but_set_variable" = xyes ; then
        COMP_VENDOR_CFLAGS="-Wunused-but-set-variable $COMP_VENDOR_CFLAGS"
    fi
    COMP_VENDOR_LDFLAGS=
    ;;
esac

COMP_VENDOR_CFLAGS="-DNDEBUG $COMP_VENDOR_CFLAGS"

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

case "${host}" in
armv7[bl] | armv7-*)
    if test "$enable_neon" = "yes" ; then
        AC_DEFINE([SPANDSP_USE_ARM_NEON], [1], [Use the NEON instruction set (ARMV7 only).])
    fi
    ;;
x86_64-* | i386-* | i686-*)
    if test "$enable_avx2" = "yes" ; then
        AC_DEFINE([SPANDSP_USE_AVX2], [1], [Use the AVX2 instruction set (i386 and x86_64 only).])
        enable_avx="yes"
    fi
    if test "$enable_avx" = "yes" ; then
        AC_DEFINE([SPANDSP_USE_AVX], [1], [Use the AVX instruction set (i386 and x86_64 only).])
        enable_sse4_2="yes"
    fi
    if test "$enable_sse4_2" = "yes" ; then
        AC_DEFINE([SPANDSP_USE_SSE4_2], [1], [Use the SSE4.2 instruction set (i386 and x86_64 only).])
        enable_sse4_1="yes"
    fi
    if test "$enable_sse4_1" = "yes" ; then
        AC_DEFINE([SPANDSP_USE_SSE4_1], [1], [Use the SSE4.1 instruction set (i386 and x86_64 only).])
        enable_ssse3="yes"
    fi
    if test "$enable_ssse3" = "yes" ; then
        AC_DEFINE([SPANDSP_USE_SSSE3], [1], [Use the SSSE3 instruction set (i386 and x86_64 only).])
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
    ;;
esac

case "$host" in
    *bsd*)
        CFLAGS="$CFLAGS -I/usr/local/include"
        LDFLAGS="$LDFLAGS -L/usr/local/lib"
    ;;
esac

if test "$enable_builtin_tiff" = "yes" ; then
    abs_tiffdir="`cd ../tiff-4.0.2/ && pwd`"
    save_CFLAGS=$CFLAGS
    CFLAGS="$CFLAGS -I$abs_tiffdir/libtiff"
    AC_CHECK_HEADERS([tiffio.h])
    AC_CHECK_HEADERS([tif_dir.h], [], [], [#include <tiffio.h>
])
    CFLAGS="$save_CFLAGS"
    COMP_VENDOR_CFLAGS="-I$abs_tiffdir/libtiff $COMP_VENDOR_CFLAGS"
    COMP_VENDOR_LDFLAGS="-L$abs_tiffdir/libtiff $COMP_VENDOR_LDFLAGS"
    TIFF_LIBS="$abs_tiffdir/libtiff/libtiff.la"
    AC_DEFINE([HAVE_LIBTIFF], [1], [Define to 1 if you have the `tiff' library (-ltiff).])
else
    AC_CHECK_HEADERS([tiffio.h])
    AC_CHECK_LIB([tiff], [TIFFOpen], , AC_MSG_ERROR("Cannot build without libtiff (does your system require a libtiff-devel package?)"), -lm)
fi

AC_CHECK_LIB([tiff], [TIFFCreateCustomDirectory], [
    if test "$ac_cv_header_tif_dir_h" = "yes" ; then
        AC_DEFINE([SPANDSP_SUPPORT_TIFF_FX], [1], [Support TIFF/FX in TIFF file handling])
        SPANDSP_SUPPORT_TIFF_FX="#define SPANDSP_SUPPORT_TIFF_FX 1"
    else
        SPANDSP_SUPPORT_TIFF_FX="#undef SPANDSP_SUPPORT_TIFF_FX"
    fi
], [SPANDSP_SUPPORT_TIFF_FX="#undef SPANDSP_SUPPORT_TIFF_FX"], -lm)

AC_CHECK_HEADERS([jpeglib.h])
AC_CHECK_LIB([jpeg], [jpeg_start_compress], [JPEG_LIBS="-ljpeg"], AC_MSG_ERROR("Cannot build without libtiff (does your system require a libjpeg-devel or libjpeg-turbo-devel package?)"))

LIBS="$LIBS $TIFF_LIBS $JPEG_LIBS"

TESTLIBS="$SIMLIBS $TESTLIBS"

if test "$enable_t43" = "yes" ; then
    AC_DEFINE([SPANDSP_SUPPORT_T43], [1], [Support T.43 JBIG gray and colour compression])
    SPANDSP_SUPPORT_T43="#define SPANDSP_SUPPORT_T43 1"
else
    SPANDSP_SUPPORT_T43="#undef SPANDSP_SUPPORT_T43"
fi

if test "$enable_v32bis" = "yes" ; then
    AC_DEFINE([SPANDSP_SUPPORT_V32BIS], [1], [Support the V.32bis modem])
    SPANDSP_SUPPORT_V32BIS="#define SPANDSP_SUPPORT_V32BIS 1"
else
    SPANDSP_SUPPORT_V32BIS="#undef SPANDSP_SUPPORT_V32BIS"
fi

if test "$enable_v34" = "yes" ; then
    AC_DEFINE([SPANDSP_SUPPORT_V34], [1], [Support the V.34 FAX modem])
    SPANDSP_SUPPORT_V34="#define SPANDSP_SUPPORT_V34 1"
else
    SPANDSP_SUPPORT_V34="#undef SPANDSP_SUPPORT_V34"
fi

AM_CONDITIONAL([COND_DOC], [test "$enable_doc" = yes])
AM_CONDITIONAL([COND_TESTS], [test "$enable_tests" = yes])
AM_CONDITIONAL([COND_MMX], [test "$enable_mmx" = yes])
AM_CONDITIONAL([COND_SSE], [test "$enable_sse" = yes])
AM_CONDITIONAL([COND_SSE2], [test "$enable_sse2" = yes])
AM_CONDITIONAL([COND_SSE3], [test "$enable_sse3" = yes])
AM_CONDITIONAL([COND_SSSE3], [test "$enable_ssse3" = yes])
AM_CONDITIONAL([COND_SSE4_1], [test "$enable_sse4_1" = yes])
AM_CONDITIONAL([COND_SSE4_2], [test "$enable_sse4_2" = yes])
AM_CONDITIONAL([COND_AVX], [test "$enable_avx" = yes])
AM_CONDITIONAL([COND_AVX2], [test "$enable_avx2" = yes])
AM_CONDITIONAL([COND_NEON], [test "$enable_neon" = yes])

AM_CONDITIONAL([COND_V32BIS], [test yes = xyes])
AM_CONDITIONAL([COND_V34], [test yes = xyes])

AC_SUBST(SPANDSP_LT_CURRENT)
AC_SUBST(SPANDSP_LT_REVISION)
AC_SUBST(SPANDSP_LT_AGE)
AC_SUBST(CC_FOR_BUILD)
AC_SUBST(CPPFLAGS_FOR_BUILD)
AC_SUBST(COMP_VENDOR_CFLAGS)
AC_SUBST(COMP_VENDOR_LDFLAGS)
AC_SUBST(SIMLIBS)
AC_SUBST(TESTLIBS)
AC_SUBST(SPANDSP_USE_FIXED_POINT)
AC_SUBST(SPANDSP_MISALIGNED_ACCESS_FAILS)
AC_SUBST(SPANDSP_USE_EXPORT_CAPABILITY)
AC_SUBST(SPANDSP_SUPPORT_T43)
AC_SUBST(SPANDSP_SUPPORT_V32BIS)
AC_SUBST(SPANDSP_SUPPORT_V34)
AC_SUBST(SPANDSP_SUPPORT_TIFF_FX)
AC_SUBST(INSERT_INTTYPES_HEADER)
AC_SUBST(INSERT_STDINT_HEADER)
AC_SUBST(INSERT_TGMATH_HEADER)
AC_SUBST(INSERT_MATH_HEADER)
AC_SUBST(INSERT_STDBOOL_HEADER)

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
                 test-data/itu/tiff-fx/Makefile
                 test-data/local/Makefile
                 tests/Makefile
                 spandsp.pc
                 spandsp.spec])

AC_OUTPUT

# @end 1

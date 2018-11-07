#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.


AC_INIT(libdingaling, 0.1)
AC_CONFIG_AUX_DIR(build)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([src/config.h])

CFLAGS="$CFLAGS $CONFIGURE_CFLAGS"
CXXFLAGS="$CXXFLAGS $CONFIGURE_CXXFLAGS"
LDFLAGS="$LDFLAGS $CONFIGURE_LDFLAGS"

#Set default language
AC_LANG_C
# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_INSTALL

#Check for compiler vendor
AX_COMPILER_VENDOR

#  Optimize
AC_ARG_ENABLE(optimization,
[AC_HELP_STRING([--enable-optimization],[Set if you want us to add max optimising compiler flags])],[enable_optimizer="$enableval"],[enable_optimizer="no"])

if test "${enable_optimizer}" = "yes" ; then
        AC_DEFINE([OPTIMZER],[],[Enable Optimization.])
        AX_CC_MAXOPT
fi

#  Enable debugging
AC_ARG_ENABLE(debug,
[AC_HELP_STRING([--enable-debug],[build with debug information])],[enable_debug="$enable_debug"],[enable_debug="yes"])

if test "${enable_debug}" = "yes"; then
        AC_DEFINE([DEBUG],[],[Enable extra debugging.])
        AX_CFLAGS_WARN_ALL_ANSI
fi

AM_CONDITIONAL([WANT_DEBUG],[test "${enable_debug}" = "yes"])


case "$host" in
                *-solaris2*)
                        if test "x${ax_cv_c_compiler_vendor}"  =  "xsun" ; then
                                SOLINK="-Bdynamic -dy -G"
                                new_AM_CFLAGS="-KPIC -DPIC"
                                new_AM_LDFLAGS="-R${prefix}/lib"
                                FUNC_DEF=__func__
                                IN_LINE=""
                        elif test "x${ax_cv_c_compiler_vendor}" = "xgnu" ; then
                                SOLINK="-Bdynamic -dy -G"
                                new_AM_CFLAGS="-fPIC"
                                new_AM_LDFLAGS=""
                                IN_LINE=inline
                        fi
                ;;
                *-darwin*)
                        if test "x${ax_cv_c_compiler_vendor}"="xgnu" ; then
                                SOLINK="-dynamic -bundle -force-flat-namespace"
                                new_AM_CFLAGS="-DMACOSX"
                                new_AM_LDFLAGS=""
                        fi
                        IN_LINE=inline
                ;;
                x86_64-*-linux-gnu)
                        if test "x${ax_cv_c_compiler_vendor}" = "xsun" ; then
                                SOLINK="-Bdynamic -dy -G"
                                new_AM_CFLAGS="-KPIC -DPIC"
                                new_AM_LDFLAGS="-R${prefix}/lib"
                                FUNC_DEF=__func__
                        elif test "x${ax_cv_c_compiler_vendor}"="xgnu" ; then
                                SOLINK="-shared -Xlinker -x"
                                new_AM_CFLAGS="-fPIC"
                                new_AM_LDFLAGS=""
                        fi
                        IN_LINE=inline
                ;;
                i*6-*-linux-gnu)
                        if test "x${ax_cv_c_compiler_vendor}" = "xsun" ; then
                                SOLINK="-Bdynamic -dy -G"
                                new_AM_CFLAGS="-KPIC -DPIC"
                                new_AM_LDFLAGS="-R${prefix}/lib"
                                FUNC_DEF=__func__
                        elif test "x${ax_cv_c_compiler_vendor}"="xgnu" ; then
                                SOLINK="-shared -Xlinker -x"
                                new_AM_CFLAGS="-fpic"
                                new_AM_LDFLAGS=""
                        fi
                        IN_LINE=inline
                ;;
                *)
                        if test "x${ax_cv_c_compiler_vendor}" = "xsun" ; then
                                SOLINK="-Bdynamic -dy -G"
                                new_AM_CFLAGS="-KPIC -DPIC"
                                new_AM_LDFLAGS="-R${prefix}/lib"
                                FUNC_DEF=__func__
                        elif test "x${ax_cv_c_compiler_vendor}"="xgnu" ; then
                                SOLINK="-shared -Xlinker -x"
                                new_AM_CFLAGS="-fPIC"
                                new_AM_LDFLAGS=""
                        fi
                        IN_LINE=inline
esac

#  Enable 64 bit build
AC_ARG_ENABLE(64,
[AC_HELP_STRING([--enable-64],[build with 64 bit support])],[enable_64="$enable_64"],[enable_64="no"])

if test "x${ax_cv_c_compiler_vendor}"  =  "xsun" ; then
    if test "${enable_64}" = "yes"; then
        new_AM_CFLAGS="$new_AM_CFLAGS -m64"
    fi
fi

AC_SUBST(new_AM_CFLAGS)
AC_SUBST(new_AM_LDFLAGS)
AC_SUBST(SOLINK)
AC_DEFINE_UNQUOTED([__inline__],[$IN_LINE],[sunpro is bad at inline])

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
#AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL
AC_FUNC_STRFTIME
#AC_CHECK_FUNCS([gethostname gettimeofday localtime_r memmove memset socket strcasecmp strchr strdup strncasecmp strstr])

AC_C_BIGENDIAN(AC_DEFINE([__BYTE_ORDER],__BIG_ENDIAN,[Big Endian]),AC_DEFINE([__BYTE_ORDER],__LITTLE_ENDIAN,[Little Endian]))
AC_DEFINE([__LITTLE_ENDIAN],1234,[for the places where it is not defined])
AC_DEFINE([__BIG_ENDIAN],4321,[for the places where it is not defined])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT

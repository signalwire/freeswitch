#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(libteletone, 0.1, BUG-REPORT-ADDRESS)
AC_CONFIG_AUX_DIR(build)
AM_INIT_AUTOMAKE(libteletone,0.1)
AC_CONFIG_SRCDIR([src])

# disable checks
m4_defun([_LT_AC_LANG_CXX_CONFIG], [:])
m4_defun([_LT_AC_LANG_F77_CONFIG], [:])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_INSTALL

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

dnl check for the compiler used
AX_COMPILER_VENDOR

case "$host" in
        *-solaris2*)
                if test "x${ax_cv_c_compiler_vendor}" = "xsun" ; then
                        new_AM_CFLAGS="-KPIC -DPIC"
                        new_AM_LDFLAGS="-R${prefix}/lib"
                fi
        ;;
        *-darwin*)
                if test "x${ax_cv_c_compiler_vendor}" = "xgnu" ; then
                        new_AM_CFLAGS="-DMACOSX"
                fi
        ;;
        x86_64-unknown-linux-gnu)
                new_AM_CFLAGS="-fPIC"
                new_AM_LDFLAGS=""
        ;;
        i*6-unknown-linux-gnu)
                new_AM_CFLAGS="-fpic"
                new_AM_LDFLAGS=""
        ;;
        x86_64-*-freebsd*|amd64-*-freebsd*)
                new_AM_CFLAGS="-fpic"
                new_AM_LDFLAGS=""
        ;;
        i*6-*-freebsd*)
                new_AM_CFLAGS="-fpic"
                new_AM_LDFLAGS=""
        ;;
esac

#
# gcc visibility cflag checks
#
AC_ARG_ENABLE([visibility],
	[AS_HELP_STRING([--disable-visibility], [Disable or enable API visibility support (default: use if available)])],
	[enable_visibility="${enableval}"],
	[enable_visibility="detect"]
)
HAVE_VISIBILITY="no"

if test "x${enable_visibility}" != "xno" ; then

	case "${ax_cv_c_compiler_vendor}" in
	gnu)
		save_CFLAGS="${CFLAGS}"
		CFLAGS="${CFLAGS} -fvisibility=hidden"
		AC_MSG_CHECKING([whether the compiler supports -fvisibility=hidden])
		AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM(
				[int foo __attribute__ ((visibility("default")));],
				[;]
			)],

			[AC_MSG_RESULT([yes])
			 new_AM_CFLAGS="${new_AM_CFLAGS} -fvisibility=hidden"
			 AC_DEFINE([HAVE_VISIBILITY], [1], [GCC visibility support available])
			 HAVE_VISIBILITY="yes"],

			[AC_MSG_RESULT([no])]
		)
		CFLAGS="${save_CFLAGS}"
		;;

	sun)
		save_CFLAGS="${CFLAGS}"
		CFLAGS="${CFLAGS} -xldscope=hidden"
		AC_MSG_CHECKING([whether the compiler supports -xldscope=hidden])
		AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM(
				[int foo __attribute__ ((visibility("default")));],
				[;]
			)],

			[AC_MSG_RESULT([yes])
			 new_AM_CFLAGS="${new_AM_CFLAGS} -xldscope=hidden"
			 AC_DEFINE([HAVE_VISIBILITY], [1], [SUNCC visibility support available])
			 HAVE_VISIBILITY="yes"],

			[AC_MSG_RESULT([no])]
		)
		CFLAGS="${save_CFLAGS}"
		;;

	*)
		if test "x${enable_visibility}" = "xyes" ; then
			AC_MSG_ERROR([Non-GNU / SUN compilers are currently unsupported])
		else
			AC_MSG_WARN([Non-GNU / SUN compilers are currently unsupported])
		fi
		;;
	esac

	#
	# visibility explicitly requested but not supported by this compiler => error
	#
	if test "x${enable_visibility}" = "xyes" -a "x${HAVE_VISIBILITY}" = "xno" ; then
		AC_MSG_ERROR([API visibility not supported by this compiler])
	fi
fi

AC_SUBST(new_AM_CFLAGS)
AC_SUBST(new_AM_LDFLAGS)

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
AC_FUNC_MALLOC
AC_TYPE_SIGNAL
AC_FUNC_STRFTIME
#AC_CHECK_FUNCS([gethostname gettimeofday localtime_r memmove memset socket strcasecmp strchr strdup strncasecmp strstr])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT

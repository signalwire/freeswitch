#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(libks, 0.1, bugs@freeswitch.org)
AC_CONFIG_AUX_DIR(build)
AC_CONFIG_MACRO_DIR([build])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src])

# disable checks
m4_defun([_LT_AC_LANG_CXX_CONFIG], [:])
m4_defun([_LT_AC_LANG_F77_CONFIG], [:])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
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
[AC_HELP_STRING([--enable-debug],[build with debug information])],[enable_debug="$enable_debug"],[enable_debug="no"])

if test "${enable_debug}" = "yes"; then
        AC_DEFINE([DEBUG],[],[Enable extra debugging.])
fi

AM_CONDITIONAL([WANT_DEBUG],[test "${enable_debug}" = "yes"])

dnl check for the compiler used
AX_COMPILER_VENDOR

case "$host" in
        *-solaris2*)
                if test "x${ax_cv_c_compiler_vendor}" = "xsun" ; then
                        AM_CFLAGS="-KPIC -DPIC"
                        AM_LDFLAGS="-R${prefix}/lib"
                fi
        ;;
        *-darwin*)
                if test "x${ax_cv_c_compiler_vendor}" = "xgnu" ; then
                        AM_CFLAGS="-DMACOSX"
                fi
        ;;
        x86_64-unknown-linux-gnu)
                AM_CFLAGS="-fPIC"
                AM_LDFLAGS=""
        ;;
        i*6-unknown-linux-gnu)
                AM_CFLAGS="-fpic"
                AM_LDFLAGS=""
        ;;
        x86_64-*-freebsd*|amd64-*-freebsd*)
                AM_CFLAGS="-fpic"
                AM_LDFLAGS=""
        ;;
        i*6-*-freebsd*)
                AM_CFLAGS="-fpic"
                AM_LDFLAGS=""
        ;;
esac

AX_CFLAGS_WARN_ALL_ANSI

AC_CHECK_LIB(rt, clock_gettime, [AC_DEFINE(HAVE_CLOCK_GETTIME, 1, [Define if you have clock_gettime()])])
AC_CHECK_LIB(rt, clock_getres, [AC_DEFINE(HAVE_CLOCK_GETRES, 1, [Define if you have clock_getres()])])
AC_CHECK_LIB(rt, clock_nanosleep, [AC_DEFINE(HAVE_CLOCK_NANOSLEEP, 1, [Define if you have clock_nanosleep()])])
AC_CHECK_FUNCS([usleep])

#
# sched_setcheduler + round-robin scheduler prerequisites
#
AC_CHECK_HEADERS([sched.h byteswap.h sys/endian.h])
AC_CHECK_DECL([SCHED_RR],
	[AC_DEFINE([HAVE_SCHED_RR],[1],[SCHED_RR constant for sched_setscheduler])],,
	[#ifdef HAVE_SCHED_H
	 #include <sched.h>
	 #endif])
AC_CHECK_FUNCS([sched_setscheduler memmem])

if test "x${ac_cv_func_sched_setscheduler}" = "xyes" -a \
        "x${ac_cv_have_decl_SCHED_RR}" = "xyes"
then
        AC_DEFINE([USE_SCHED_SETSCHEDULER],[1],[Enable round-robin scheduler using sched_setscheduler])
	AM_CFLAGS="${AM_CFLAGS} -DUSE_SCHED_SETSCHEDULER=1"
fi



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
			 AM_CFLAGS="${AM_CFLAGS} -DKS_API_VISIBILITY=1 -fvisibility=hidden"
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
			 AM_CFLAGS="${AM_CFLAGS} -DKS_API_VISIBILITY=1 -xldscope=hidden"
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

AM_CFLAGS="${AM_CFLAGS} -Werror"
AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_LDFLAGS)

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC

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

AC_CHECK_LIB(pthread, pthread_setschedparam, [
AC_DEFINE(HAVE_PTHREAD_SETSCHEDPARAM, 1, [Define if you have pthread_setschedparam()])
AM_CFLAGS="${AM_CFLAGS} -DHAVE_PTHREAD_SETSCHEDPARAM=1"
])

AC_C_BIGENDIAN(AC_DEFINE([__BYTE_ORDER],__BIG_ENDIAN,[Big Endian]),AC_DEFINE([__BYTE_ORDER],__LITTLE_ENDIAN,[Little Endian]))
AC_DEFINE([__LITTLE_ENDIAN],1234,[for the places where it is not defined])
AC_DEFINE([__BIG_ENDIAN],4321,[for the places where it is not defined])

path_remove () {
  echo "$1" | tr ':' '\n' | grep -Fxv "$2" | tr '\n' ':' | sed 's/:$//'
}
path_push_unique () {
  x="$(eval echo \$$1)"
  x="$(path_remove "$x" "$2")"
  if test -z "$x"; then
    eval export $1="$2"
  else
    eval export $1="$2:$x"
  fi
}

case $host in
   *-darwin*)
        path_push_unique PKG_CONFIG_PATH /usr/local/opt/openssl/lib/pkgconfig
       ;;
esac

SAC_OPENSSL

if test x$HAVE_OPENSSL = x1; then
   	openssl_CFLAGS="$openssl_CFLAGS -DHAVE_OPENSSL";
	AM_CFLAGS="${AM_CFLAGS} ${openssl_CFLAGS} -DHAVE_OPENSSL"
	AM_LDFLAGS="${AM_LDFLAGS} ${openssl_LIBS}"
else
	AC_MSG_ERROR([OpenSSL and associated developement headers required])
fi


#  Enable clang address sanitizer bit build
AC_ARG_ENABLE(address_sanitizer,
  [AC_HELP_STRING([--enable-address-sanitizer],[build with address sanitizer])],
  [enable_address_sanitizer="$enable_address_sanitizer"],
  [enable_address_sanitizer="no"])

if test "${enable_address_sanitizer}" = "yes"; then
  if test "x${ax_cv_c_compiler_vendor}"  =  "xclang" ; then
    AM_CFLAGS="${AM_CFLAGS} -fsanitize=address -fno-omit-frame-pointer"
    AM_CXXFLAGS="${AM_CXXFLAGS} -fsanitize=address -fno-omit-frame-pointer"
    AM_LDFLAGS="${AM_LDFLAGS} -fsanitize=address"
  fi
fi

PKG_CHECK_MODULES([SODIUM], [libsodium >= 1.0.0],[AC_MSG_RESULT([yes])],[AC_MSG_ERROR([libsodium is required])])
PKG_CHECK_MODULES([UUID], [uuid >= 1.0.0],[AC_MSG_RESULT([yes])],[AC_MSG_ERROR([libuuid is required])])

AM_CFLAGS="${AM_CFLAGS} -Werror ${SODIUM_CFLAGS} ${UUID_CFLAGS}"
AM_LDFLAGS="${AM_LDFLAGS} ${SODIUM_LIBS} ${UUID_LIBS}"

AC_CONFIG_FILES([Makefile
	test/Makefile
	libks.pc
])

AC_OUTPUT

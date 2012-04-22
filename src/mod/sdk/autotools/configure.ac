#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# website url
m4_define([AC_PACKAGE_URL], [http://www.example.com/])

AC_PREREQ([2.61])
AC_INIT([mod_example], [0.0.0], [contact@example.com])
AC_CONFIG_SRCDIR([src/mod_example.c])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign no-dist subdir-objects])
AC_DISABLE_STATIC

# disable libtool fortran and c++ checks
m4_defun([_LT_AC_LANG_F77_CONFIG], [:])
m4_defun([_LT_AC_LANG_CXX_CONFIG], [:])

# >=automake-1.11
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CC_C99
AC_PROG_LIBTOOL

# pkgconfig
AC_PATH_PROG([PKG_CONFIG], [pkg-config], ["no"])
AS_IF([test "x${PKG_CONFIG}" = "xno"],
	[AC_MSG_ERROR([Cannot find pkg-config, make sure it is installed in your PATH])]
)
PKG_PROG_PKG_CONFIG


##
## Checks for cflags
##
AC_MSG_RESULT([${as_nl}<<>> Compiler vendor and features])

AX_COMPILER_VENDOR

AM_CONDITIONAL([CC_VENDOR_GNU], [test "x${ax_cv_c_compiler_vendor}" = "xgnu"])
AM_CONDITIONAL([CC_VENDOR_SUN], [test "x${ax_cv_c_compiler_vendor}" = "xsun"])


##
## Compiler vendor and flag checks
##
HAVE_VISIBILITY="no"

AC_ARG_ENABLE([visibility],
	[AS_HELP_STRING([--disable-visibility], [Disable symbol visibility support (default: enabled, if available)])],
	[case "${enableval}" in
	 yes|no) enable_visibility="${enableval}" ;;
	 *)      AC_MSG_ERROR([Invalid value ${enableval} for parameter --disable-visibility]) ;;
	 esac],
	[enable_visibility="yes"]
)

case "${ax_cv_c_compiler_vendor}" in
gnu)
	AC_MSG_CHECKING([whether the compiler supports -fvisibility=hidden])
	AS_IF([test "x${enable_visibility}" != "xno"],
		[save_CFLAGS="${CFLAGS}"
		 CFLAGS="${CFLAGS} -fvisibility=hidden"
		 AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM(
				[int foo __attribute__ ((visibility("default")));],
				[;]
			)],

			[HAVE_VISIBILITY="yes"
			 AC_DEFINE([HAVE_VISIBILITY], [1], [Symbol visibility support])
			 AC_SUBST([VISIBILITY_CFLAGS],   [-fvisibility=hidden])
			 AC_SUBST([VISIBILITY_CXXFLAGS], [-fvisibility=hidden])],
			[HAVE_VISIBILITY="no"]
		 )
		 AC_MSG_RESULT([${HAVE_VISIBILITY}])
		 CFLAGS="${save_CFLAGS}"],
		[AC_MSG_RESULT([disabled by user])]
	)
	;;
sun)
	AC_MSG_CHECKING([whether the compiler supports -xldscope=hidden])
	AS_IF([test "x${enable_visibility}" != "xno"],
		[save_CFLAGS="${CFLAGS}"
		 CFLAGS="${CFLAGS} -xldscope=hidden"
		 AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM(
				[int foo __attribute__ ((visibility("default")));],
				[;]
			)],

			[HAVE_VISIBILITY="yes"
			 AC_DEFINE([HAVE_VISIBILITY], [1], [Symbol visibility support])
			 AC_SUBST([VISIBILITY_CFLAGS],   [-xldscope=hidden])
			 AC_SUBST([VISIBILITY_CXXFLAGS], [-xldscope=hidden])],
			[HAVE_VISIBILITY="no"]
		 )
		 AC_MSG_RESULT([${HAVE_VISIBILITY}])
		 CFLAGS="${save_CFLAGS}"],
		[AC_MSG_RESULT([disabled by user])]
	)
	;;
*)
	AC_MSG_WARN([No visibility checks for this compiler defined])
	;;
esac

AM_CONDITIONAL([CC_HAS_VISIBILITY], [test "x${HAVE_VISIBILITY}" = "xyes"])


##
## pkgconfig based freeswitch detection code
##
AC_MSG_RESULT([${as_nl}<<>> FreeSWITCH environment])

PKG_CHECK_MODULES([freeswitch], [freeswitch],
	[save_LIBS="${LIBS}"
	 save_CFLAGS="${CFLAGS}"
	 save_CPPFLAGS="${CPPFLAGS}"

	 AC_MSG_CHECKING([FreeSWITCH version])

	 FREESWITCH_VERSION="`${PKG_CONFIG} --modversion freeswitch 2>/dev/null`"
	 AS_IF([test "x${FREESWITCH_VERSION}" = "x"],
		[AC_MSG_ERROR([failed to get FreeSWITCH version])],
		[AC_MSG_RESULT([$FREESWITCH_VERSION])]
	 )

	 AC_MSG_CHECKING([whether FreeSWITCH ${FREESWITCH_VERSION} is usable])
	 CFLAGS="${freeswitch_CFLAGS}"
	 CPPFLAGS="${freeswitch_CPPFLAGS}"
	 LIBS="${freeswitch_LIBS}"
	 AC_TRY_LINK([#include <switch.h>],
		[switch_core_init(0, 0, NULL);],
		[AC_MSG_RESULT([yes])],
		[AC_MSG_RESULT([no])
		 AC_MSG_ERROR([libfreeswitch is unusable, please check config.log for details])]
	 )
	 LIBS="${save_LIBS}"
	 CFLAGS="${save_CFLAGS}"
	 CPPFLAGS="${save_CPPFLAGS}"

	 # get locations, critical first
	 AC_MSG_CHECKING([for installation prefix])
	 FREESWITCH_PREFIX_DIR="`${PKG_CONFIG} --variable=prefix freeswitch 2>/dev/null`"
	 AS_IF(
		[test "x${FREESWITCH_PREFIX_DIR}" = "x"], [AC_MSG_ERROR([unable to get FreeSWITCH prefix directory])],
		[test ! -e "${FREESWITCH_PREFIX_DIR}"], [AC_MSG_ERROR([FreeSWITCH prefix directory ${FREESWITCH_PREFIX_DIR} does not exist])]
	 )
	 AC_MSG_RESULT([${FREESWITCH_PREFIX_DIR}])

	 AC_MSG_CHECKING([for modules directory])
	 FREESWITCH_MODULES_DIR="`${PKG_CONFIG} --variable=modulesdir freeswitch 2>/dev/null`"
	 AS_IF(
		[test "x${FREESWITCH_MODULES_DIR}" = "x"], [AC_MSG_ERROR([unable to get FreeSWITCH modules directory])],
		[test ! -e "${FREESWITCH_MODULES_DIR}"], [AC_MSG_ERROR([FreeSWITCH modules directory ${FREESWITCH_MODULES_DIR} does not exist])]
	 )
	 AC_MSG_RESULT([${FREESWITCH_MODULES_DIR}])

	 AC_MSG_CHECKING([for configuration directory])
	 FREESWITCH_CONFIG_DIR="`${PKG_CONFIG} --variable=sysconfdir freeswitch 2>/dev/null`"
	 AS_IF(
		[test "x${FREESWITCH_CONFIG_DIR}" = "x"], [AC_MSG_ERROR([unable to get FreeSWITCH configuration directory])],
		[test ! -e "${FREESWITCH_CONFIG_DIR}"], [AC_MSG_ERROR([FreeSWITCH configuration directory ${FREESWITCH_CONFIG_DIR} does not exist])]
	 )
	 AC_MSG_RESULT([${FREESWITCH_CONFIG_DIR}])

	 # non-critical paths
	 FREESWITCH_HTDOCS_DIR="`${PKG_CONFIG} --variable=htdocsdir freeswitch 2>/dev/null`"

	 FREESWITCH_RUNTIME_DIR="`${PKG_CONFIG} --variable=runtimedir freeswitch 2>/dev/null`"
	 FREESWITCH_SCRIPTS_DIR="`${PKG_CONFIG} --variable=scriptsdir freeswitch 2>/dev/null`"

	 # cflags, libs
	 AC_SUBST([FREESWITCH_CFLAGS],   [${freeswitch_CFLAGS}])
	 AC_SUBST([FREESWITCH_CPPFLAGS], [${freeswitch_CPPFLAGS}])
	 AC_SUBST([FREESWITCH_LIBS],     [${freeswitch_LIBS}])

	 # version
	 AC_SUBST([FREESWITCH_VERSION])

	 # locations
	 AC_SUBST([FREESWITCH_PREFIX_DIR])
	 AC_SUBST([FREESWITCH_HTDOCS_DIR])
	 AC_SUBST([FREESWITCH_CONFIG_DIR])
	 AC_SUBST([FREESWITCH_MODULES_DIR])
	 AC_SUBST([FREESWITCH_SCRIPTS_DIR])
	 AC_SUBST([FREESWITCH_RUNTIME_DIR])
	],
	[AC_MSG_ERROR([FreeSWITCH not found])]
)


##
## Add your other dependency checks here
##
AC_MSG_RESULT([${as_nl}<<>> Other dependencies])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.


##
## Done, create output files and print summary
##
AC_MSG_RESULT([${as_nl}<<>> Create output files])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

AC_MSG_RESULT([
====================== Configuration Summary =====================
+ Package
	Name:...................... ${PACKAGE_NAME}
	Version:................... ${PACKAGE_VERSION}
	Bugreports:................ ${PACKAGE_BUGREPORT}
	Website:................... ${PACKAGE_URL}

+ Compiler
	Vendor:.................... ${ax_cv_c_compiler_vendor}
	Symbol visibility:......... ${HAVE_VISIBILITY}

+ FreeSWITCH
	Version:................... ${FREESWITCH_VERSION}
	Prefix:.................... ${FREESWITCH_PREFIX_DIR}
	Modules directory:......... ${FREESWITCH_MODULES_DIR}
	Configuration directory:... ${FREESWITCH_CONFIG_DIR}

	Cflags/CPPflags/CXXflags:.. ${FREESWITCH_CFLAGS} ${FREESWITCH_CPPFLAGS}
	LDflags/Libs:.............. ${FREESWITCH_LIBS} ${FREESWITCH_LDFLAGS}

+ Other
	N/A
==================================================================
])

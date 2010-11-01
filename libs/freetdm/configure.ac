#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([freetdm], [pre-alpha], [bugs@freeswitch.org])
AC_CONFIG_SRCDIR([src/ftdm_io.c])

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build])
AM_INIT_AUTOMAKE([libfreetdm], [0.1])

# >=automake-1.11
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# override some default libtool behavior and invoke AC_PROG_LIBTOOL
# (see http://lists.gnu.org/archive/html/libtool/2007-03/msg00000.html)
m4_defun([_LT_AC_LANG_F77_CONFIG], [:])
m4_defun([_LT_AC_LANG_GCJ_CONFIG], [:])
m4_defun([_LT_AC_LANG_RC_CONFIG], [:])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG

AX_COMPILER_VENDOR

# AC_PREFIX_DEFAULT does not get expanded until too late so we need
# to do this to use prefix in this script
AC_PREFIX_DEFAULT([/usr/local/freetdm])
if test "x${prefix}" = "xNONE" ; then
	prefix='/usr/local/freetdm'
fi

# Absolute source/build directory
FT_SRCDIR=`(cd $srcdir && pwd)`
ft_builddir=`pwd`
AC_SUBST([FT_SRCDIR])
AC_SUBST([ft_builddir])

if test "$sysconfdir" = "\${prefix}/etc" ; then
	confdir="$prefix/conf"
else
	confdir="$sysconfdir"
fi

AC_SUBST([confdir])

# Where to install the modules
AC_ARG_WITH([modinstdir],
	[AS_HELP_STRING([--with-modinstdir=DIR], [Install modules into this location (default: ${prefix}/mod)])],
	[case "${withval}" in
	 no|yes) AC_MSG_ERROR([Invalid value \"${withval}\", option requires a valid path]) ;;
	 *)	 modinstdir="${withval}" ;;
	 esac],
	[modinstdir="${prefix}/mod"]
)
AC_SUBST([modinstdir])

AC_ARG_ENABLE([enable_64],
	[AS_HELP_STRING([--enable-64], [Enable 64bit compilation])],
	[enable_64="${enableval}"],
	[enable_64="no"]
)

case "${ax_cv_c_compiler_vendor}" in
gnu)
	COMP_VENDOR_CFLAGS="-ffast-math -Wall -Werror -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -O0"
	;;
sun)
	COMP_VENDOR_CFLAGS="-xc99=all -mt -xCC -D__FUNCTION__=__func__ -xvpara"
	if test "$enable_64" != "no" ; then
		COMP_VENDOR_CFLAGS="-m64 $COMP_VENDOR_CFLAGS"
	fi
	;;
*)
	COMP_VENDOR_CFLAGS="-std=c99 -Wall -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes"
	;;
esac
AC_SUBST([COMP_VENDOR_CFLAGS])

#  Enable debugging
AC_ARG_ENABLE([debug],
	[AC_HELP_STRING([--enable-debug], [build with debug information])],
	[enable_debug="${enableval}"],
	[enable_debug="yes"]
)
if test "${enable_debug}" != "no"; then
        AC_DEFINE([DEBUG], [], [Enable extra debugging.])

	if test "x${ax_cv_c_compiler_vendor}" = "xgnu" ; then
		COMP_VENDOR_CFLAGS="$COMP_VENDOR_CFLAGS -g -ggdb"
	fi
fi
AC_SUBST([COMP_VENDOR_CFLAGS])

# set DYNAMIC_LIB_EXTEN
# we should really be using libtool so we don't need to do this
case "$host" in
*cygwin* | *mingw*)
	DYNAMIC_LIB_EXTEN="dll"
	;;
*)
	DYNAMIC_LIB_EXTEN="so"
	;;
esac
AC_SUBST([DYNAMIC_LIB_EXTEN])

AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([m], [cos])

AC_CHECK_HEADERS([netinet/sctp.h netdb.h sys/select.h])
AM_CONDITIONAL([HAVE_SCTP],[test "${ac_cv_header_netinet_sctp_h}" = "yes"])

AC_CHECK_FUNC([gethostbyname_r],
	[], [AC_CHECK_LIB([nsl], [gethostbyname_r])]
)
if test "$ac_cv_func_gethostbyname_r" = "yes" -o "$ac_cv_lib_nsl_gethostbyname_r" = "yes"
then
	AC_MSG_CHECKING([whether gethostbyname_r requires five arguments])

	ac_cv_func_gethostbyname_r_five_args="no"

	AC_TRY_COMPILE([#include <netdb.h>],
		[char *name;
		 struct hostent *he, *res;
		 char buffer[2048];
		 int buflen = 2048;
		 (void)gethostbyname_r(name, he, buffer, buflen, &res)],
		[ac_cv_func_gethostbyname_r_five_args="yes"
	         AC_DEFINE([HAVE_GETHOSTBYNAME_R_FIVE], [1], [gethostbyname_r has five arguments])]
	)

	AC_MSG_RESULT([$ac_cv_func_gethostbyname_r_five_args])
	AC_DEFINE([HAVE_GETHOSTBYNAME_R], [1], [threadsafe gethostbyname])
fi

##
## Modules and optional features
##
AC_MSG_RESULT([${as_nl}<<>> Modules and optional features])

##
# libpri (TODO: add checks)
#
HAVE_LIBPRI="no"
AC_ARG_WITH([libpri],
	[AS_HELP_STRING([--with-libpri], [Install ftmod_libpri])],
	[case "${withval}" in
	 no)	enable_libpri="no" ;;
	 *)	enable_libpri="yes" ;;
	 esac],
	[enable_libpri="no"]
)
HAVE_LIBPRI="${enable_libpri}"
AM_CONDITIONAL([HAVE_LIBPRI],[test "${enable_libpri}" = "yes"])

##
# pritap (TODO: add checks)
#
HAVE_PRITAP="no"
AC_ARG_WITH([pritap],
	[AS_HELP_STRING([--with-pritap], [Install ftmod_pritap])],
	[case "${withval}" in
	 no)	enable_pritap="no" ;;
	 *)	enable_pritab="yes" ;;
	 esac],
	[enable_pritap="no"]
)
HAVE_PRITAP="${enable_pritap}"
AM_CONDITIONAL([HAVE_PRITAP],[test "${enable_pritap}" = "yes"])

# debug dtmf?
AC_ARG_WITH([debugdtmf],
	[AS_HELP_STRING([--with-debugdtmf], [Debug DTMF])],
	[case "${withval}" in
	 no)	enable_debugdtmf="no" ;;
	 *)	enable_debugdtmf="yes" ;;
	 esac],
	[enable_debugdtmf="no"]
)
AM_CONDITIONAL([WANT_DEBUGDTMF], [test "${enable_debugdtmf}" = "yes"])

##
# OpenR2 stack
#
HAVE_OPENR2="no"
AC_CHECK_LIB([openr2], [openr2_context_set_io_type], [HAVE_OPENR2="yes"])
AC_MSG_RESULT([checking whether to build ftmod_r2... ${HAVE_OPENR2}])
AM_CONDITIONAL([HAVE_OPENR2], [test "${HAVE_OPENR2}" = "yes"])

##
# Sangoma Wanpipe
#
HAVE_LIBSANGOMA="no"
AC_MSG_RESULT([${as_nl}<<>> Sangoma Wanpipe])
AC_CHECK_LIB([sangoma], [sangoma_span_chan_toif], [HAVE_LIBSANGOMA="yes"])
AC_MSG_RESULT([checking whether to build ftmod_wanpipe... ${HAVE_LIBSANGOMA}])
AM_CONDITIONAL([HAVE_LIBSANGOMA], [test "${HAVE_LIBSANGOMA}" = "yes"])

##
# Sangoma ISDN stack
#
HAVE_SNG_ISDN="no"
AC_MSG_RESULT([${as_nl}<<>> Sangoma ISDN stack])
AC_CHECK_LIB([sng_isdn], [sng_isdn_init], [HAVE_SNG_ISDN="yes"])
AC_MSG_RESULT([checking whether to build ftmod_sangoma_isdn... ${HAVE_SNG_ISDN}])
AM_CONDITIONAL([HAVE_SNG_ISDN], [test "${HAVE_SNG_ISDN}" = "yes"])

if test "${HAVE_SNG_ISDN}" = "yes"; then
	if test "${build}" == "${host}"
	then
		case "${host}" in
		x86_64-*)
			# X86_64 machines need additional flags when compiling against libsng_isdn
			CFLAGS="$CFLAGS -DBIT_64 -DALIGN_64BIT"
			;;
		esac
	fi
fi

##
# Sangoma SS7 stack
#
HAVE_SNG_SS7="no"
AC_MSG_RESULT([${as_nl}<<>> Sangoma SS7 stack])
AC_CHECK_LIB([sng_ss7], [sng_isup_init], [HAVE_SNG_SS7="yes"])
AC_MSG_RESULT([checking whether to build ftmod_sangoma_ss7... ${HAVE_SNG_SS7}])
AM_CONDITIONAL([HAVE_SNG_SS7], [test "${HAVE_SNG_SS7}" = "yes"])

if test "${HAVE_SNG_SS7}" = "yes"; then
	if test "${build}" == "${host}"
	then
		case "${host}" in
		x86_64-*)
			# X86_64 machines need additional flags when compiling against libsng_isdn
			CFLAGS="$CFLAGS -DBIT_64 -DALIGN_64BIT"
			;;
		esac
	fi
fi

##
# libisdn
#
HAVE_LIBISDN="no"
AC_ARG_WITH([libisdn],
	[AS_HELP_STRING([--with-libisdn], [Install ftmod_isdn (libisdn stack)])],
	[case "${withval}" in
	 no)	with_libisdn="no" ;;
	 *)	with_libisdn="yes" ;;
	 esac
	],
	[with_libisdn="no"]
)
if test "${with_libisdn}" != "no"
then
	AC_MSG_RESULT([${as_nl}<<>> ftmod_isdn (libisdn stack)])
	PKG_CHECK_MODULES([libisdn],
		[libisdn >= 0.0.1],
		[AC_MSG_CHECKING([libisdn version])
		 LIBISDN_VERSION="`${PKG_CONFIG} --modversion libisdn`"
		 if test -z "${LIBISDN_VERSION}"; then
			AC_MSG_ERROR([Failed to retrieve libisdn version])
		 fi
		 AC_MSG_RESULT([${LIBISDN_VERSION}])

		 # check features
		 AC_MSG_CHECKING([for new experimental API])
		 AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM(
				[#include <libisdn/version.h>
				 #if !LIBISDN_FEATURE(API2)
				 #error "libisdn API v2 not available"
				 #endif
				],
				[;]
			)],
			[AC_MSG_RESULT([yes])],
			[AC_MSG_RESULT([no])]
		 )

		 HAVE_LIBISDN="yes"
		 AC_DEFINE([HAVE_LIBISDN], [1], [libisdn support])
		 AC_SUBST([LIBISDN_CFLAGS],  [${libisdn_CFLAGS}])
		 AC_SUBST([LIBISDN_CPPFLAGS],[${libisdn_CPPFLAGS}])
		 AC_SUBST([LIBISDN_LDFLAGS], [${libisdn_LDFLAGS}])
		 AC_SUBST([LIBISDN_LIBS],    [${libisdn_LIBS}])
		 AC_SUBST([LIBISDN_VERSION])
		],
		[AC_MSG_ERROR([Need libisdn-0.0.1 or higher])]
	)
	AX_LIB_PCAP
fi
AM_CONDITIONAL([HAVE_LIBISDN], [test "${HAVE_LIBISDN}" = "yes"])

AC_MSG_RESULT([${as_nl}<<>> Creating output files])
AC_CONFIG_FILES([
	Makefile
	freetdm.pc
	mod_freetdm/Makefile
])
AC_OUTPUT

#
# summary screen
#
AC_MSG_RESULT([
============================ FreeTDM configuration ============================

+ Modules

  Signalling:
	ftmod_isdn......................... ${HAVE_LIBISDN}
	ftmod_libpri....................... ${HAVE_LIBPRI}
	ftmod_sangoma_isdn................. ${HAVE_SNG_ISDN}
	ftmod_sangoma_ss7.................. ${HAVE_SNG_SS7}
	ftmod_r2........................... ${HAVE_OPENR2}
	ftmod_pritap....................... ${HAVE_PRITAP}
  I/O:
	ftmod_wanpipe...................... ${HAVE_LIBSANGOMA}

===============================================================================
])

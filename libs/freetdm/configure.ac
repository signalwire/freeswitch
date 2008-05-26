#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([openzap],[pre-alpha],[bugs@freeswitch.org])
AC_CONFIG_SRCDIR([src/libteletone_detect.c])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Check for com;iler type
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
AC_ARG_ENABLE([enable_64], [AS_HELP_STRING([--enable-64], [Enable 64bit compilation])], [enable_64="$enableval"], [enable_64="no"])

AX_COMPILER_VENDOR

case "${ax_cv_c_compiler_vendor}" in
gnu)
    COMP_VENDOR_CFLAGS="-ffast-math -Wall -Werror -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes"
    ;;
sun)
    COMP_VENDOR_CFLAGS="-xc99=all -mt -xCC -D__FUNCTION__=__func__ -xvpara"
    if test "$enable_64" = "yes" ; then
        COMP_VENDOR_CFLAGS="-m64 $COMP_VENDOR_CFLAGS"
    fi
    ;;
*)
    COMP_VENDOR_CFLAGS="-std=c99 -Wall -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes"
    ;;
esac

AC_CHECK_HEADERS([netinet/sctp.h netdb.h])
AC_CHECK_FUNC([gethostbyname_r],
	[], [AC_CHECK_LIB([nsl], [gethostbyname_r])]
)
if test "$ac_cv_func_gethostbyname_r" = "yes" -o "$ac_cv_lib_nsl_gethostbyname_r" = "yes" ; then

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
    AC_DEFINE([HAVE_GETHOSTBYNAME_R],[1],[threadsafe gethostbyname])
fi

#  Enable debugging
AC_ARG_ENABLE(debug,
[AC_HELP_STRING([--enable-debug],[build with debug information])],[enable_debug="$enableval"],[enable_debug="yes"])

if test "${enable_debug}" = "yes"; then
        AC_DEFINE([DEBUG],[],[Enable extra debugging.])

	if test "x${ax_cv_c_compiler_vendor}" = "xgnu" ; then
	   COMP_VENDOR_CFLAGS="$COMP_VENDOR_CFLAGS -g -ggdb"
	fi
fi



COMP_VENDOR_CFLAGS="$COMP_VENDOR_CFLAGS"
AC_SUBST(COMP_VENDOR_CFLAGS)
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

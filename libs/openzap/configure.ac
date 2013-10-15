#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([openzap],[pre-alpha],[bugs@freeswitch.org])
AC_CONFIG_SRCDIR([src/zap_io.c])

AC_CONFIG_AUX_DIR(build)
AM_INIT_AUTOMAKE(libopenzap,0.1)

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AM_PROG_CC_C_O

AC_PREFIX_DEFAULT(/usr/local/openzap)
# AC_PREFIX_DEFAULT does not get expanded until too late so we need to do this to use prefix in this script                                                     
if test "x$prefix" = "xNONE" ; then
    prefix='/usr/local/openzap'
fi

# Absolute source/build directory
OZ_SRCDIR=`(cd $srcdir && pwd)`
oz_builddir=`pwd`
AC_SUBST(OZ_SRCDIR)
AC_SUBST(oz_builddir)

if test "$sysconfdir" = "\${prefix}/etc" ; then
    confdir="$prefix/conf"
else
    confdir="$sysconfdir"
fi

AC_SUBST(confdir)

#override some default libtool behavior and invoke AC_PROG_LIBTOOL (see http://lists.gnu.org/archive/html/libtool/2007-03/msg00000.html)
m4_defun([_LT_AC_LANG_F77_CONFIG], [:])
m4_defun([_LT_AC_LANG_GCJ_CONFIG], [:])
m4_defun([_LT_AC_LANG_RC_CONFIG], [:])
#AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_PROG_INSTALL

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
    COMP_VENDOR_CFLAGS="-ffast-math -Wall -Werror -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -O0"
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


#set SOLINK variable based on compiler and host
if test "x${ax_cv_c_compiler_vendor}"  =  "xsun" ; then
    SOLINK="-Bdynamic -dy -G"
elif test "x${ax_cv_c_compiler_vendor}" = "xclang" ; then
    case "$host" in
        *darwin*)
            SOLINK="-dynamic -bundle -force-flat-namespace"
        ;;
        *)
            AC_ERROR([Please update configure.in with SOLINK values for your compiler])
        ;;
    esac
elif test "x${ax_cv_c_compiler_vendor}" = "xgnu" ; then
    case "$host" in
        *darwin*)
            SOLINK="-dynamic -bundle -force-flat-namespace"
        ;;
        *-solaris2*)
            SOLINK="-shared -Xlinker"
        ;;
        *)
            SOLINK="-shared -Xlinker -x"
        ;;
    esac
else
    AC_ERROR([Please update configure.in with SOLINK values for your compiler])
fi

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

DEFAULT_INCLUDES="-I. -I./src/include -I$(srcdir)"

AC_SUBST(SOLINK)
AC_SUBST(DEFAULT_INCLUDES)
AC_SUBST(DYNAMIC_LIB_EXTEN)

AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([m], [cos])
AX_LIB_PCAP

AC_CHECK_HEADERS([netinet/sctp.h netdb.h sys/select.h])
AM_CONDITIONAL([HAVE_SCTP],[test "${ac_cv_header_netinet_sctp_h}" = "yes"])

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

# Where to install the modules
AC_ARG_WITH([modinstdir],
 [AS_HELP_STRING([--with-modinstdir=DIR], [Install modules into this location (default: $prefix/mod)])], [modinstdir="$withval"], [modinstdir="${prefix}/mod"])

AC_SUBST(modinstdir)


# libpri?
AC_ARG_WITH([libpri],
 [AS_HELP_STRING([--with-libpri], [Install ozmod_libpri])], [enable_libpri="yes"], [enable_libpri="no"])
AC_SUBST(enable_libpri)

# debug dtmf?
AC_ARG_WITH([debugdtmf],
 [AS_HELP_STRING([--with-debugdtmf], [Debug DTMF])], [enable_debugdtmf="yes"], [enable_debugdtmf="no"])
AC_SUBST(enable_debugdtmf)

AC_CHECK_LIB([sangoma], [sangoma_span_chan_toif], [have_libsangoma="yes"])
AM_CONDITIONAL([LIBSANGOMA],[test "${have_libsangoma}" = "yes"])

AM_CONDITIONAL([LIBPRI],[test "${enable_libpri}" = "yes"])

AM_CONDITIONAL([DEBUGDTMF],[test "${enable_debugdtmf}" = "yes"])

AC_CHECK_LIB([openr2], [openr2_context_set_io_type], [have_openr2="yes"])
AM_CONDITIONAL([OPENR2],[test "${have_openr2}" = "yes"])

COMP_VENDOR_CFLAGS="$COMP_VENDOR_CFLAGS"
AC_SUBST(COMP_VENDOR_CFLAGS)
AC_CONFIG_FILES([Makefile
		openzap.pc
		mod_openzap/Makefile])
AC_OUTPUT

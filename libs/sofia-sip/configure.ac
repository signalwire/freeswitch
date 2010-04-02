dnl Copyright (C) 2005-2006 Nokia Corporation
dnl Contact: Pekka Pessi <pekka.pessi@nokia.com>
dnl Licensed under LGPL. See file COPYING.

dnl
dnl ref: http://www.gnu.org/software/autoconf/manual/autoconf.info.gz
dnl
AC_PREREQ(2.57)

dnl information on the package
dnl ---------------------------

dnl update both the version for AC_INIT and the LIBSOFIA_SIP_UA_MAJOR_MINOR
AC_INIT([sofia-sip], [1.12.10devel])

CFLAGS="$CFLAGS $CONFIGURE_CFLAGS"
CXXFLAGS="$CXXFLAGS $CONFIGURE_CXXFLAGS"
LDFLAGS="$LDFLAGS $CONFIGURE_LDFLAGS"

AC_CONFIG_SRCDIR([libsofia-sip-ua/sip/sofia-sip/sip.h])
AC_CONFIG_MACRO_DIR([m4])
AC_SUBST(VER_LIBSOFIA_SIP_UA_MAJOR_MINOR, [1.12])
dnl Includedir specific to this sofia version
AC_SUBST(include_sofiadir, '${includedir}/sofia-sip-1.12')
AC_SUBST(LIBVER_SOFIA_SIP_UA_CUR, [6])
AC_SUBST(LIBVER_SOFIA_SIP_UA_REV, [0])
AC_SUBST(LIBVER_SOFIA_SIP_UA_AGE, [6])
AC_SUBST(LIBVER_SOFIA_SIP_UA_SOVER, [0]) # CUR-AGE
AC_SUBST(LIBVER_SOFIA_SIP_UA_GLIB_CUR, [3])
AC_SUBST(LIBVER_SOFIA_SIP_UA_GLIB_REV, [0])
AC_SUBST(LIBVER_SOFIA_SIP_UA_GLIB_AGE, [0])
AC_SUBST(LIBVER_SOFIA_SIP_UA_GLIB_SOVER, [3]) # CUR-AGE

# dnl calls AC_CANONICAL_ macros that are required by AM_INIT_AUTOMAKE
SAC_CANONICAL_SYSTEM_CACHE_CHECK

AM_INIT_AUTOMAKE

AC_CONFIG_HEADERS([config.h])

AC_GNU_SOURCE

CFLAGS="$CFLAGS -DSU_DEBUG=0 $DEBUG_CFLAGS"
### checks for programs
### -------------------
AC_LANG([C])
AC_CHECK_COMPILATION_ENVIRONMENT

SAC_TOOL_CC
AC_PROG_INSTALL
AC_PROG_CPP
AC_CHECK_PROG(ETAGS, etags, etags, echo)
AC_CHECK_TOOL(AR, ar, ar)
AC_CHECK_TOOL(LD, ld, ld)
AC_PROG_LIBTOOL

SAC_ENABLE_NDEBUG
SAC_ENABLE_EXPENSIVE_CHECKS

dnl Add parameters for aclocal
AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")

AC_ARG_WITH(doxygen,
[  --with-doxygen[[=CMD]]    use doxygen command CMD [[doxygen]]],[
case $enable_doxygen in
yes ) doxygen=doxygen ;;
no ) doxygen=echo ;;
esac], doxygen=doxygen)

AC_CHECK_PROG([DOXYGEN], [doxygen], [$doxygen], [echo])
AM_CONDITIONAL([HAVE_DOXYGEN], [test $DOXYGEN != echo])

AC_DEFUN([AX_COMPILER_VENDOR],
[
AC_CACHE_CHECK([for _AC_LANG compiler vendor], ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor,
 [ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor=unknown
  # note: don't check for gcc first since some other compilers define __GNUC__
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

AX_COMPILER_VENDOR

#  Enable 64 bit build
AC_ARG_ENABLE(64,
[AC_HELP_STRING([--enable-64],[build with 64 bit support])],[enable_64="$enable_64"],[enable_64="no"])

if test "x${ax_cv_c_compiler_vendor}"  =  "xsun" ; then
    if test "${enable_64}" = "yes"; then
        SOFIA_CFLAGS="$SOFIA_CFLAGS -m64"
	LDFLAGS="-m64 -Wl,-64"
	export PKG_CONFIG_PATH=/usr/lib/64/pkgconfig
    fi
fi

SOFIA_PLAT_CFLAGS=
# openbsd seems to not define NULL as a void pointer, I blame standards by committee for this.
# This is a dirty hack, but shuts up all the warnings
case "$host" in
     *-openbsd*) SOFIA_PLAT_CFLAGS="-DNULL='(void *) 0L'";;
     *) ;;
esac

AC_SUBST(SOFIA_PLAT_CFLAGS, $SOFIA_PLAT_CFLAGS)


### checks for header files
### -----------------------
AC_HEADER_STDC

### checks for declarations
### -----------------------

### checks for types
### ----------------

AC_TYPE_SIGNAL
AC_TYPE_LONGLONG

dnl
dnl Define HAVE_C99_FORMAT to 1 if the formatted IO functions (printf/scanf
dnl et.al.) support the C99 'size specifiers', namely ll, hh, j, z, t
dnl (representing long long int, char, intmax_t, size_t, ptrdiff_t). Some C
dnl compilers supported these specifiers prior to C99 as an extension.
dnl
AC_CACHE_CHECK([whether IO functions support C99 size specifiers],
[ac_cv_c_c99_format],[

ac_cv_c_c99_format=yes

AC_RUN_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT],
[[char buf[64];
 if (sprintf(buf, "%lld%hhd%jd%zd%td", (long long int)1, (char)2, (intmax_t)3, (size_t)4, (ptrdiff_t)5) != 5)
  exit(1);
 else if (strcmp(buf, "12345"))
  exit(2);]])],
 [ac_cv_c_c99_format=yes],
 [ac_cv_c_c99_format=no],
 [ac_cv_c_c99_format=yes])
])

if test $ac_cv_c_c99_format = yes; then
  AC_DEFINE([HAVE_C99_FORMAT], [1], [Define to 1 if printf supports C99 size specifiers])dnl

  AC_DEFINE([LLU], ["%llu"], [Format (%llu) for unsigned long long])dnl
  AC_DEFINE([LLI], ["%lli"], [Format (%lli) for long long])dnl
  AC_DEFINE([LLX], ["%llx"], [Format (%llx) for long long hex])dnl
  AC_DEFINE([MOD_ZD], ["%zd"], [Define printf() modifier for ssize_t])dnl
  AC_DEFINE([MOD_ZU], ["%zu"], [Define printf() modifier for size_t])dnl

else

AC_CACHE_CHECK([whether IO functions support size specifier for long long],
[ac_cv_c_ll_format],[

ac_cv_c_ll_format=yes

AC_RUN_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT],
[[char buf[64];
 if (sprintf(buf, "%lld", (long long int)1) != 1)
  exit(1);
 else if (strcmp(buf, "1"))
  exit(2);]])],
 [ac_cv_c_ll_format=yes],
 [ac_cv_c_ll_format=no],
 [ac_cv_c_ll_format=yes])
])

if test $ac_cv_c_ll_format = yes; then
  AC_DEFINE([LLU], ["%llu"], [Format (%llu) for unsigned long long])dnl
  AC_DEFINE([LLI], ["%lli"], [Format (%lli) for long long])dnl
  AC_DEFINE([LLX], ["%llx"], [Format (%llx) for long long hex])dnl
else
  AC_MSG_ERROR("printf cannot handle 64-bit integers")
fi

AC_CACHE_CHECK([whether IO functions support size specifier for size_t],
[ac_cv_c_z_format],[

ac_cv_c_z_format=yes

AC_RUN_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT],
[[char buf[64];
 if (sprintf(buf, "%zd", (size_t)1) != 1)
  exit(1);
 else if (strcmp(buf, "1"))
  exit(2);]])],
 [ac_cv_c_z_format=yes],
 [ac_cv_c_z_format=no],
 [ac_cv_c_z_format=yes])
])

if test $ac_cv_c_z_format = yes; then
  AC_DEFINE([MOD_ZD], ["%ld"], [Define printf() modifier for ssize_t])dnl
  AC_DEFINE([MOD_ZU], ["%lu"], [Define printf() modifier for size_t])dnl
else
  dnl Cross fingers
  AC_MSG_WARN("printf cannot handle size_t, using long instead")
  AC_DEFINE([MOD_ZD], ["%ld"], [Define printf() modifier for ssize_t])dnl
  AC_DEFINE([MOD_ZU], ["%lu"], [Define printf() modifier for size_t])dnl
fi

fi

### checks for structures
### ---------------------

### checks for typedefs, structures, and compiler characteristics.
### --------------------------------------------------------------
AC_C_CONST
AC_C_INLINE
AC_C_INLINE_DEFINE
AC_C_BIGENDIAN

AC_C_VAR_FUNC
AC_C_MACRO_FUNCTION
AC_C_KEYWORD_STRUCT

AC_HEADER_TIME
AC_TYPE_SIZE_T

AC_SYS_SA_LEN



### checks for library functions
### ----------------------------

### checks for system services
### --------------------------

AC_DEV_URANDOM

### checks for libraries
### --------------------

SAC_SOFIA_SU
SAC_OPENSSL
SAC_TPORT

dnl Check is used for testing
PKG_CHECK_MODULES(CHECK, check >= 0.9.4, have_check="yes", have_check="no")
AM_CONDITIONAL(HAVE_CHECK, test x"$have_check" = "xyes")
if test x"$have_check" = "xyes"; then
   AC_DEFINE([HAVE_CHECK], 1, [Define to 1 if check library is available])
fi
AC_CHECK_HEADERS([fnmatch.h])

dnl dl is currently used only in testing
AC_CHECK_LIB([dl], [dlopen], [
   dnl Note: -ldl is not added to LIBS
   AC_DEFINE([HAVE_LIBDL], 1, [Define to 1 if dl library is available])
])

### internal modules
### ----------------
AC_DEFINE([HAVE_SOFIA_SIP], 1, [Define to 1 always])
AC_DEFINE([HAVE_SOFIA_SRESOLV], 1, [Define to 1 if we use DNS library])
AC_DEFINE([HAVE_SOFIA_SMIME], 0, [Define to 1 if we use S/MIME library])

AC_ARG_ENABLE(stun,
[  --disable-stun          disable stun module (enabled)],
 , enable_stun=yes)

if test x$enable_stun = xno ; then
  AC_MSG_WARN([** STUN support disabled **])
elif test x${HAVE_OPENSSL} != x1 ; then
  dnl compile STUN only if OPENSSL is available
  AC_MSG_WARN([** TLS support for STUN disabled as OpenSSL headers and/or libraries were not found **])
  AC_DEFINE([HAVE_SOFIA_STUN], 1, [Define to 1 if we use STUN library])
else
  AC_DEFINE([HAVE_SOFIA_STUN], 1, [Define to 1 if we use STUN library])
fi
AM_CONDITIONAL([HAVE_STUN], [test "x$enable_stun" = xyes])

AC_ARG_ENABLE(nth,
[  --disable-nth           disable HTTP-related modules nth and http (enabled)],
 , enable_nth=yes)
AM_CONDITIONAL([HAVE_NTH], [test "x$enable_nth" = xyes])
if test x$enable_nth = xyes ; then
  AC_DEFINE([HAVE_SOFIA_NTH], 1, [Define to 1 if we use NTH library])
  AC_DEFINE([HAVE_SOFIA_HTTP], 1, [Define to 1 if we use HTTP parser library])
fi

dnl Disable NTLM support by default
AC_ARG_ENABLE(ntlm,
[  --enable-ntlm           enable NTLM support [[disabled]]],
 , enable_ntlm=no)

if test x$enable_ntlm = xyes ; then
  AC_DEFINE([HAVE_SOFIA_NTLM], 1, [Define to 1 if we use NTLM library])
fi
AM_CONDITIONAL([HAVE_NTLM], [test "x$enable_ntlm" = xyes])


AC_DEFINE([HAVE_SRTP], 0, [Define to 1 if we use SRTP])
AC_DEFINE([HAVE_UPNP], 0, [Define to 1 if we use UPnP])


AC_ARG_ENABLE(memleak_log,
[  --enable-memleak-log    enable logging of possible memory leaks [[disabled]]],
 , enable_memleak_log=no)

if test x$enable_memleak_log = xyes ; then
  AC_DEFINE([HAVE_MEMLEAK_LOG], 1, [Define to 1 for memory-leak-related logging])
fi

### output
### ------

AC_CONFIG_FILES([
Makefile
packages/Makefile
packages/sofia-sip-ua.pc
packages/sofia-sip-ua-glib.pc
libsofia-sip-ua/Makefile
libsofia-sip-ua/bnf/Makefile
libsofia-sip-ua/bnf/Doxyfile
libsofia-sip-ua/docs/Makefile
libsofia-sip-ua/docs/Doxyfile
libsofia-sip-ua/docs/Doxyfile.version
libsofia-sip-ua/docs/sofia-footer.html
libsofia-sip-ua/features/Doxyfile
libsofia-sip-ua/features/Makefile
libsofia-sip-ua/http/Doxyfile
libsofia-sip-ua/http/Makefile
libsofia-sip-ua/ipt/Doxyfile
libsofia-sip-ua/ipt/Makefile
libsofia-sip-ua/iptsec/Doxyfile
libsofia-sip-ua/iptsec/Makefile
libsofia-sip-ua/msg/Doxyfile
libsofia-sip-ua/msg/Makefile
libsofia-sip-ua/nea/Doxyfile
libsofia-sip-ua/nea/Makefile
libsofia-sip-ua/nta/Doxyfile
libsofia-sip-ua/nta/Makefile
libsofia-sip-ua/nth/Doxyfile
libsofia-sip-ua/nth/Makefile
libsofia-sip-ua/nua/Doxyfile
libsofia-sip-ua/nua/Makefile
libsofia-sip-ua/sdp/Doxyfile
libsofia-sip-ua/sdp/Makefile
libsofia-sip-ua/sip/Doxyfile
libsofia-sip-ua/sip/Makefile
libsofia-sip-ua/soa/Doxyfile
libsofia-sip-ua/soa/Makefile
libsofia-sip-ua/sresolv/Doxyfile
libsofia-sip-ua/sresolv/Makefile
libsofia-sip-ua/stun/Doxyfile
libsofia-sip-ua/stun/Makefile
libsofia-sip-ua/su/Doxyfile
libsofia-sip-ua/su/Makefile
libsofia-sip-ua/tport/Doxyfile
libsofia-sip-ua/tport/Makefile
libsofia-sip-ua/url/Doxyfile
libsofia-sip-ua/url/Makefile
libsofia-sip-ua/features/sofia-sip/sofia_features.h
s2check/Makefile
libsofia-sip-ua-glib/Makefile
libsofia-sip-ua-glib/su-glib/Makefile
libsofia-sip-ua-glib/su-glib/Doxyfile
utils/Makefile
utils/Doxyfile
tests/Makefile
win32/Makefile
win32/config.h
open_c/Makefile
open_c/config.h
])

AC_CONFIG_COMMANDS([version],,[PACKAGE_VERSION=${PACKAGE_VERSION}])
AC_CONFIG_FILES([packages/sofia-sip-${PACKAGE_VERSION}.spec:packages/sofia-sip.spec.in])

AC_OUTPUT

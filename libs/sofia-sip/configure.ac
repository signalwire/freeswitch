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
AC_INIT([sofia-sip], [1.12.5])
AC_CONFIG_SRCDIR([libsofia-sip-ua/sip/sofia-sip/sip.h])
AC_SUBST(VER_LIBSOFIA_SIP_UA_MAJOR_MINOR, [1.12])
dnl Includedir specific to this sofia version
AC_SUBST(include_sofiadir, '${includedir}/sofia-sip-1.12')
AC_SUBST(LIBVER_SOFIA_SIP_UA_CUR, [4])
AC_SUBST(LIBVER_SOFIA_SIP_UA_REV, [0])
AC_SUBST(LIBVER_SOFIA_SIP_UA_AGE, [4])
AC_SUBST(LIBVER_SOFIA_SIP_UA_SOVER, [0]) # CUR-AGE
AC_SUBST(LIBVER_SOFIA_SIP_UA_GLIB_CUR, [3])
AC_SUBST(LIBVER_SOFIA_SIP_UA_GLIB_REV, [0])
AC_SUBST(LIBVER_SOFIA_SIP_UA_GLIB_AGE, [0])
AC_SUBST(LIBVER_SOFIA_SIP_UA_GLIB_SOVER, [3]) # CUR-AGE
					    
# dnl calls AC_CANONICAL_ macros that are required by AM_INIT_AUTOMAKE
SAC_CANONICAL_SYSTEM_CACHE_CHECK

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_CONFIG_HEADERS([config.h])

AC_GNU_SOURCE

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

AC_CHECK_PROG([DOXYGEN], [doxygen], [doxygen], [echo])
AM_CONDITIONAL([HAVE_DOXYGEN], [test $DOXYGEN = doxygen])

### checks for libraries
### --------------------

SAC_SOFIA_SU
SAC_OPENSSL
SAC_TPORT
SAC_SU

### internal modules
### ----------------
AC_DEFINE([HAVE_SOFIA_SIP], 1, [Define to 1 always])
AC_DEFINE([HAVE_SOFIA_SRESOLV], 1, [Define to 1 if we use DNS library])
AC_DEFINE([HAVE_SOFIA_SMIME], 0, [Define to 1 if we use S/MIME library])

AC_ARG_ENABLE(stun,
[  --disable-stun              disable stun module (enabled)],
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
[  --disable-nth              disable nth and http modules (enabled)],
 , enable_nth=yes)
AM_CONDITIONAL([HAVE_NTH], [test "x$enable_nth" = xyes])
if test x$enable_nth = xyes ; then
  AC_DEFINE([HAVE_SOFIA_NTH], 1, [Define to 1 if we use NTH library])
fi

dnl Disable NTLM support by default
AC_ARG_ENABLE(ntlm,
[  --enable-ntlm               enable NTLM support (disabled)],
 , enable_ntlm=no)

if test x$enable_ntlm = xyes ; then
  AC_DEFINE([HAVE_SOFIA_NTLM], 1, [Define to 1 if we use NTLM library])
fi
AM_CONDITIONAL([HAVE_NTLM], [test "x$enable_ntlm" = xyes])


AC_DEFINE([HAVE_SRTP], 0, [Define to 1 if we use SRTP])
AC_DEFINE([HAVE_UPNP], 0, [Define to 1 if we use UPnP])

### checks for header files
### -----------------------
AC_HEADER_STDC

### checks for declarations
### -----------------------

### checks for types
### ----------------

AC_TYPE_SIGNAL
AC_TYPE_LONGLONG([
AC_DEFINE([LLU], ["%llu"], [Define to format (%llu) for unsigned long long])dnl
AC_DEFINE([LLI], ["%lli"], [Define to format (%lli) for long long])dnl
AC_DEFINE([LLX], ["%llx"], [Define to format (%llx) for long long hex])dnl
])
AC_DEFINE([MOD_ZD], ["%zd"], [Define printf() modifier for ssize_t])
AC_DEFINE([MOD_ZU], ["%zu"], [Define printf() modifier for size_t])

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

AC_STRUCT_SIN6
AC_SYS_SA_LEN

### checks for library functions
### ----------------------------

### checks for system services
### --------------------------

AC_DEV_URANDOM

### output
### ------

AC_CONFIG_FILES([
Makefile
packages/Makefile
packages/sofia-sip-ua.pc
packages/sofia-sip-ua-glib.pc
libsofia-sip-ua/Makefile
libsofia-sip-ua/bnf/Makefile
libsofia-sip-ua/docs/Makefile
libsofia-sip-ua/docs/Doxyfile.version
libsofia-sip-ua/docs/sofia-footer.html
libsofia-sip-ua/features/Makefile
libsofia-sip-ua/http/Makefile
libsofia-sip-ua/ipt/Makefile
libsofia-sip-ua/iptsec/Makefile
libsofia-sip-ua/msg/Makefile
libsofia-sip-ua/nea/Makefile
libsofia-sip-ua/nta/Makefile
libsofia-sip-ua/nth/Makefile
libsofia-sip-ua/nua/Makefile
libsofia-sip-ua/sdp/Makefile
libsofia-sip-ua/sip/Makefile
libsofia-sip-ua/soa/Makefile
libsofia-sip-ua/sresolv/Makefile 
libsofia-sip-ua/stun/Makefile
libsofia-sip-ua/su/Makefile
libsofia-sip-ua/tport/Makefile
libsofia-sip-ua/url/Makefile
libsofia-sip-ua/features/sofia-sip/sofia_features.h
libsofia-sip-ua-glib/Makefile
libsofia-sip-ua-glib/su-glib/Makefile
utils/Makefile
utils/Doxyfile.build
win32/Makefile
win32/config.h
])

AC_CONFIG_COMMANDS([version],,[PACKAGE_VERSION=${PACKAGE_VERSION}])
AC_CONFIG_FILES([packages/sofia-sip-${PACKAGE_VERSION}.spec:packages/sofia-sip.spec.in])

AC_OUTPUT

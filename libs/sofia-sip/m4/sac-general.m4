dnl =====================================================================
dnl This file contains autoconf macros shared by Sofia modules.
dnl
dnl Author: Pekka Pessi <Pekka.Pessi@nokia.com>
dnl
dnl License:
dnl
dnl Copyright (c) 2001,2004 Nokia and others. All Rights Reserved.
dnl
dnl Please note that every macro contained in this file is copyrighted by
dnl its respective author, unless the macro source explicitely says
dnl otherwise. Permission has been granted, though, to use and distribute
dnl all macros under the following license, which is a modified version of
dnl the GNU General Public License version 2:
dnl
dnl Each Autoconf macro in this file is free software; you can redistribute it
dnl and/or modify it under the terms of the GNU General Public License as
dnl published by the Free Software Foundation; either version 2, or (at your
dnl option) any later version.
dnl
dnl They are distributed in the hope that they will be useful, but WITHOUT ANY
dnl WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
dnl FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
dnl details. (You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software Foundation,
dnl Inc., 59 Temple Place -- Suite 330, Boston, MA 02111-1307, USA.)
dnl
dnl As a special exception, the Free Software Foundation gives unlimited
dnl permission to copy, distribute and modify the configure scripts that are
dnl the output of Autoconf. You need not follow the terms of the GNU General
dnl Public License when using or distributing such scripts, even though
dnl portions of the text of Autoconf appear in them. The GNU General Public
dnl License (GPL) does govern all other use of the material that constitutes
dnl the Autoconf program.
dnl
dnl Certain portions of the Autoconf source text are designed to be copied
dnl (in certain cases, depending on the input) into the output of Autoconf.
dnl We call these the "data" portions. The rest of the Autoconf source text
dnl consists of comments plus executable code that decides which of the data
dnl portions to output in any given case. We call these comments and
dnl executable code the "non-data" portions. Autoconf never copies any of
dnl the non-data portions into its output.
dnl
dnl This special exception to the GPL applies to versions of Autoconf
dnl released by the Free Software Foundation. When you make and distribute a
dnl modified version of Autoconf, you may extend this special exception to
dnl the GPL to apply to your modified version as well, *unless* your
dnl modified version has the potential to copy into its output some of the
dnl text that was the non-data portion of the version that you started with.
dnl (In other words, unless your change moves or copies text from the
dnl non-data portions to the data portions.) If your modification has such
dnl potential, you must delete any notice of this special exception to the
dnl GPL from your modified version.
dnl
dnl =====================================================================

dnl ===================================================================
dnl Get host, target and build variables filled with appropriate info,
dnl and check the validity of the cache
dnl ===================================================================

AC_DEFUN([SAC_CANONICAL_SYSTEM_CACHE_CHECK], [

dnl Get host, target and build variables filled with appropriate info.

AC_CANONICAL_TARGET([])

dnl Check to assure the cached information is valid.

AC_MSG_CHECKING(cached information)
hostcheck="$host"
AC_CACHE_VAL(ac_cv_hostcheck, [ ac_cv_hostcheck="$hostcheck" ])
if test "$ac_cv_hostcheck" != "$hostcheck"; then
  AC_MSG_RESULT(changed)
  AC_MSG_WARN(config.cache exists!)
  AC_MSG_ERROR([you must do 'make distclean' first to compile
 for different host or different parameters.])
else
  AC_MSG_RESULT(ok)
fi

dnl SOSXXX: AC_PATH_ADJUST

])

dnl ======================================================================
dnl Find C compiler
dnl ======================================================================

AC_DEFUN([SAC_TOOL_CC], [
AC_REQUIRE([SAC_CANONICAL_SYSTEM_CACHE_CHECK])
#AC_BEFORE([$0], [AC_PROG_CPP])dnl

AC_CHECK_TOOL(CC, gcc, gcc)
if test -z "$CC"; then
  AC_CHECK_TOOL(CC, cc, cc, , , /usr/ucb/cc)
  if test -z "$CC"; then
    case "`uname -s`" in
    *win32* | *WIN32* ) AC_CHECK_PROG(CC, cl, cl) ;;
    esac
  fi
  test -z "$CC" && AC_MSG_ERROR([no acceptable cc found in \$PATH])
fi

AC_PROG_CC

#
# Wall
#
AC_CACHE_CHECK([for maximum warnings compiler flag],
  ac_cv_cwflag,
[case "${CC-cc}" in
  *gcc*) ac_cv_cwflag=-Wall;;
  *)	case "$host" in
    *irix*)	ac_cv_cwflag=-fullwarn ;;
    *solaris*)  ac_cv_cwflag="-erroff=%none,E_END_OF_LOOP_CODE_NOT_REACHED,E_BAD_PTR_INT_COMBINATION -errtags"
	        ;;
    *)		ac_cv_cwflag=;;
		esac
  ;;
esac])
AC_SUBST([CWFLAG], [$ac_cv_cwflag])

AC_ARG_VAR([SOFIA_CFLAGS], [CFLAGS not used during configure])
AC_ARG_VAR([SOFIA_GLIB_CFLAGS], [Extra CFLAGS for libsofia-sip-ua-glib])

SAC_COVERAGE
])

dnl ======================================================================
dnl Check for sockaddr_in6
dnl ======================================================================
AC_DEFUN([AC_STRUCT_SIN6], [
AC_CACHE_CHECK([for sockaddr_in6],
  ac_cv_sin6,
[AC_EGREP_HEADER(struct sockaddr_in6, netinet/in.h,
  ac_cv_sin6=yes, ac_cv_sin6=no)])
if test $ac_cv_sin6 = yes ;then
	AC_DEFINE([HAVE_SIN6], 1,
		[Define to 1 if you have IPv6 structures and constants])
fi
])

dnl ======================================================================
dnl Check for sa_len in struct sockaddr
dnl ======================================================================
AC_DEFUN([AC_SYS_SA_LEN], [
AC_CACHE_CHECK([for sa_len],
  ac_cv_sa_len,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#include <sys/socket.h>]], [[
 struct sockaddr t;t.sa_len = 0;]])],[ac_cv_sa_len=yes],[ac_cv_sa_len=no])])
if test "$ac_cv_sa_len" = yes ;then
	AC_DEFINE([HAVE_SA_LEN], 1,
		[Define to 1 if you have sa_len in struct sockaddr])
fi
])

dnl ======================================================================
dnl Check for MSG_NOSIGNAL flag
dnl ======================================================================
AC_DEFUN([AC_FLAG_MSG_NOSIGNAL], [
AC_REQUIRE([AC_PROG_CC])
AC_CACHE_CHECK([for MSG_NOSIGNAL],
  ac_cv_flag_msg_nosignal, [
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/socket.h>]], [[
  int flags = MSG_NOSIGNAL;
]])],[ac_cv_flag_msg_nosignal=yes],[ac_cv_flag_msg_nosignal=no])])
if test "$ac_cv_flag_msg_nosignal" = yes ; then
	AC_DEFINE([HAVE_MSG_NOSIGNAL], 1,
		[Define to 1 if you have MSG_NOSIGNAL flag for send()])
fi
])dnl

dnl ======================================================================
dnl Check for MSG_ERRQUEUE flag
dnl ======================================================================
AC_DEFUN([AC_SYS_MSG_ERRQUEUE], [
AC_REQUIRE([AC_PROG_CC])
AC_CACHE_CHECK([for MSG_ERRQUEUE],
  ac_cv_flag_msg_errqueue, [
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/socket.h>]], [[
  int flags = MSG_ERRQUEUE;
]])],[ac_cv_flag_msg_errqueue=yes],[ac_cv_flag_msg_errqueue=no])])
if test "$ac_cv_flag_msg_errqueue" = yes; then
	AC_DEFINE([HAVE_MSG_ERRQUEUE], 1,
		[Define to 1 if you have MSG_ERRQUEUE flag for send()])
fi
])dnl

dnl ======================================================================
dnl Check for IP_RECVERR option
dnl ======================================================================
AC_DEFUN([AC_SYS_IP_RECVERR], [
AC_REQUIRE([AC_PROG_CC])
AC_CACHE_CHECK([for IP_RECVERR],
  ac_cv_sys_ip_recverr, [
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
]], [[
  int one = 1;
  int s = 0;
  setsockopt(s, SOL_IP, IP_RECVERR, &one, sizeof(one));
]])],[ac_cv_sys_ip_recverr=yes],[ac_cv_sys_ip_recverr=no])])
if test "$ac_cv_sys_ip_recverr" = yes ; then
	AC_DEFINE([HAVE_IP_RECVERR], 1,
		[Define to 1 if you have IP_RECVERR in <netinet/in.h>])
fi
])dnl

dnl ======================================================================
dnl Check for IPV6_RECVERR option
dnl ======================================================================
AC_DEFUN([AC_SYS_IPV6_RECVERR], [
AC_REQUIRE([AC_PROG_CC])
AC_CACHE_CHECK([for IPV6_RECVERR],
  ac_cv_sys_ipv6_recverr, [
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
]], [[
  int one = 1;
  int s = 0;
  setsockopt(s, SOL_IPV6, IPV6_RECVERR, &one, sizeof(one));
]])],[ac_cv_sys_ipv6_recverr=yes],[ac_cv_sys_ipv6_recverr=no])])
if test "$ac_cv_sys_ipv6_recverr" = yes ; then
	AC_DEFINE([HAVE_IPV6_RECVERR], 1,
		[Define to 1 if you have IPV6_RECVERR in <netinet/in6.h>])
fi
])dnl

dnl ======================================================================
dnl @synopsis AC_C_VAR_FUNC
dnl
dnl This macro tests if the C compiler supports the C99 standard
dnl __func__ indentifier.
dnl
dnl The new C99 standard for the C language stipulates that the
dnl identifier __func__ shall be implictly declared by the compiler
dnl as if, immediately following the opening brace of each function
dnl definition, the declaration
dnl
dnl     static const char __func__[] = "function-name";
dnl
dnl appeared, where function-name is the name of the function where
dnl the __func__ identifier is used.
dnl
dnl @author Christopher Currie <christopher@currie.com>
dnl @author Pekka Pessi <Pekka.Pessi@nokia.com>
dnl ======================================================================

AC_DEFUN([AC_C_VAR_FUNC],
[AC_REQUIRE([AC_PROG_CC])
AC_CACHE_CHECK(whether $CC recognizes __func__, ac_cv_c_var_func,
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[char *s = __func__;
]])],[ac_cv_c_var_func=yes],[ac_cv_c_var_func=no]))
if test $ac_cv_c_var_func = "yes"; then
AC_DEFINE([HAVE_FUNC], 1, [Define to 1 if the C compiler supports __func__])
fi
])dnl

AC_DEFUN([AC_C_MACRO_FUNCTION],
[AC_REQUIRE([AC_PROG_CC])
AC_CACHE_CHECK(whether $CC recognizes __FUNCTION__, ac_cv_c_macro_function,
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
char *s = __FUNCTION__;
]])],[ac_cv_c_macro_function=yes],[ac_cv_c_macro_function=no]))
if test $ac_cv_c_macro_function = "yes"; then
AC_DEFINE([HAVE_FUNCTION], 1, [Define to 1 if the C compiler supports __FUNCTION__])
fi
])dnl

dnl ======================================================================
dnl AC_C_INLINE_DEFINE
dnl ======================================================================
AC_DEFUN([AC_C_INLINE_DEFINE], [
AC_C_INLINE
case "$ac_cv_c_inline" in *inline* | yes)
	AC_DEFINE([HAVE_INLINE], 1, [Define to 1 if you have inlining compiler]) ;;
esac
])

dnl ======================================================================
dnl AC_C_KEYWORD_STRUCT
dnl ======================================================================
AC_DEFUN([AC_C_KEYWORD_STRUCT], [
AC_REQUIRE([AC_PROG_CC])
AC_CACHE_CHECK(whether $CC recognizes field names in struct initialization, ac_cv_c_keyword_struct,
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
  struct { int foo; char *bar; } test = { foo: 1, bar: "bar" };
  return 0;
]])],[ac_cv_c_keyword_struct=yes],[ac_cv_c_keyword_struct=no]))
if test $ac_cv_c_keyword_struct = "yes"; then
AC_DEFINE([HAVE_STRUCT_KEYWORDS], 1, [
Define to 1 if your CC supports C99 struct initialization])
fi
])

dnl ======================================================================
dnl autoconf-version
dnl should be removed after automake conversion is finished
dnl ======================================================================

AC_DEFUN([AC_AUTOCONF_PARAM],
[
if autoconf --version | fgrep '2.13' > /dev/null ; then
    AUTOCONF_PARAM="-l"
else
    AUTOCONF_PARAM="-I"
fi
AC_SUBST(AUTOCONF_PARAM)
])

dnl ======================================================================
dnl SAC_ENABLE_NDEBUG
dnl ======================================================================
AC_DEFUN([SAC_ENABLE_NDEBUG],[
AC_REQUIRE([SAC_TOOL_CC])
AC_ARG_ENABLE(ndebug,
[  --enable-ndebug         compile with NDEBUG [[disabled]]],
 , enable_ndebug=no)
AM_CONDITIONAL(NDEBUG, test x$enable_ndebug = xyes)
])

dnl ======================================================================
dnl SAC_ENABLE_EXPENSIVE_CHECKS
dnl ======================================================================
AC_DEFUN([SAC_ENABLE_EXPENSIVE_CHECKS],[
AC_ARG_ENABLE(expensive-checks,
[  --enable-expensive-checks
                          run also expensive checks [[disabled]]],
 , enable_expensive_checks=no)
if test x$enable_expensive_checks != xno; then
AC_SUBST([TESTS_ENVIRONMENT], [EXPENSIVE_CHECKS=1])
fi
AM_CONDITIONAL(EXPENSIVE_CHECKS, test x$enable_expensive_checks != xno)
])


dnl ======================================================================
dnl Check if we are using Windows with MinGW compiler
dnl ======================================================================

AC_DEFUN([AC_CHECK_COMPILATION_ENVIRONMENT], [
AC_REQUIRE([AC_PROG_CC])
AC_CACHE_CHECK([for compilation environment],
  ac_cv_cc_environment, [
machine=`$CC -dumpmachine`
if test "$machine" = mingw32 ; then
  ac_cc_environment=$machine
fi
])

if test "$ac_cc_environment" = mingw32 ; then
CFLAGS="$CFLAGS -I\$(top_srcdir)/win32/pthread -DWINVER=0x0501 \
	-D_WIN32_WINNT=0x0501 -DIN_LIBSOFIA_SIP_UA -DIN_LIBSOFIA_SRES \
	-mms-bitfields \
	-pipe -mno-cygwin -mwindows -mconsole -Wall -g -O0"
LDFLAGS="$LDFLAGS -Wl,--enable-auto-image-base"
LIBS="-L\$(top_srcdir)/win32/pthread -lpthreadVC2 -lws2_32 \
	-lwsock32"
MINGW_ENVIRONMENT=1
AC_SUBST(MINGW_ENVIRONMENT)
AC_DEFINE([HAVE_MINGW], [1], [Define to 1 if you are compiling in MinGW environment])
AC_DEFINE([HAVE_WIN32], [1], [Define to 1 if you have WIN32])
fi
AM_CONDITIONAL([HAVE_MINGW32], [test "x$ac_cc_environment" != x])
])dnl


dnl ======================================================================
dnl Find long long (at least 64 bits)
dnl ======================================================================

AC_DEFUN([AC_TYPE_LONGLONG],[dnl
AC_CHECK_TYPE([long long],[dnl
AC_DEFINE([longlong], [long long], [Define to a at least 64-bit int type])dnl
ifelse([$1], ,:, [$1])],[ifelse([$2], ,:, [$2])])])

dnl ======================================================================
dnl Check for /dev/urandom
dnl ======================================================================

AC_DEFUN([AC_DEV_URANDOM],[
AC_CACHE_CHECK([/dev/urandom], [ac_cv_dev_urandom],
  [ac_cv_dev_urandom=no
   if test -r /dev/urandom; then ac_cv_dev_urandom=yes; fi])
if test $ac_cv_dev_urandom = yes; then
  AC_DEFINE([HAVE_DEV_URANDOM], 1,
    [Define to 1 if you have /dev/urandom.])
  AC_DEFINE([DEV_URANDOM], 1,
    [Define to the random number source name.])
fi
])

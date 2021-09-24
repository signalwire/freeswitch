dnl ======================================================================
dnl SAC_TPORT - perform checks for tport
dnl ======================================================================
AC_DEFUN([SAC_TPORT], [

AC_ARG_WITH(sigcomp,
[  --with-sigcomp=dir      use Sofia SigComp package [[not used]]],,
	with_sigcomp=no)

if test -n "${with_sigcomp}" && test "${with_sigcomp}" != no ; then
	if test "${with_sigcomp}" != yes ; then
		CPPFLAGS="-I${with_sigcomp}/include $CPPFLAGS"
		LIBS="-L${with_sigcomp}/lib -lsigcomp $LIBS"
	else
		LIBS="-lsigcomp $LIBS"
	fi

	AC_CHECK_HEADERS(sigcomp.h,,AC_MSG_ERROR([cannot find Sofia SigComp includes]))

	AC_CHECK_FUNC(sigcomp_library_2_5,
            [AC_DEFINE([HAVE_SIGCOMP], 1, [Define to 1 if you have Sofia sigcomp >= 2.5])
             AC_DEFINE([HAVE_SOFIA_SIGCOMP], 1, [Define to 1 if you have Sofia sigcomp >= 2.5])],
             AC_MSG_ERROR(Sofia SigComp API >= 2.5 was not found))
fi

# Check for features used by tport.
AC_SYS_IP_RECVERR
AC_SYS_IPV6_RECVERR

AC_CHECK_HEADERS([netinet/tcp.h netinet/sctp.h],[],[],[
#include <sys/types.h>
#include <sys/socket.h>
])

AC_ARG_ENABLE(sctp,
[  --enable-sctp           use SCTP [[disabled]]],,
 enable_sigcomp=no)

if test x$enable_sctp = xyes; then
AC_DEFINE(HAVE_SCTP, 1, [Define to 1 if you have SCTP])
fi
])

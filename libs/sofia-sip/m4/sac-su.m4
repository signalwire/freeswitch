dnl ======================================================================
dnl su module
dnl ======================================================================

dnl The macro SAC_SOFIA_SU is in a separate file, sofia-su2.m4, because
dnl SAC_SOFIA_SU creates a separate configuration file <su/su_configure.h>. 
dnl If SAC_SOFIA_SU is included to a aclocal.m4 of another package,
dnl autoheader returns a spurious error and automake complains about missing
dnl su/su_configure.h.

dnl ======================================================================
dnl SAC_WITH_RT - check for POSIX realtime library
dnl ======================================================================
AC_DEFUN([SAC_WITH_RT],[
AC_ARG_WITH(rt,  
[  --with-rt               use POSIX realtime library [[used by default]]])
])

dnl ======================================================================
dnl SAC_CHECK_SU_LIBS - check for libraries used by su
dnl ======================================================================
AC_DEFUN([SAC_CHECK_SU_LIBS], [
AC_REQUIRE([SAC_WITH_RT])
AC_CHECK_LIB(pthread, pthread_create)
AC_CHECK_LIB(socket, socketpair,,,-lnsl)
if test "${with_rt}" != no; then
	AC_SEARCH_LIBS(clock_gettime, rt)
fi
])dnl

dnl ======================================================================
dnl SAC_SU - main macro for checking su dependencies
dnl ======================================================================
AC_DEFUN([SAC_SU], [
AC_REQUIRE([SAC_CHECK_SU_LIBS])
])

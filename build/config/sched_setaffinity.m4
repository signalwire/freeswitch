AC_DEFUN([AX_HAVE_CPU_SET], [
#
# Check for the Linux functions for controlling processor affinity.
#
# LINUX: sched_setaffinity
AC_CHECK_FUNCS(sched_setaffinity sched_getaffinity)
	if test "$ac_cv_func_sched_setaffinity" = "yes" ; then
        AC_CACHE_CHECK([whether the CPU_SET and CPU_ZERO macros are defined],
        ac_cv_cpu_set_defined,[
	saved_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $SWITCH_AM_CFLAGS $SWITCH_ANSI_CFLAGS -D_GNU_SOURCE"
        AC_TRY_COMPILE( [#include <sched.h>],[ cpu_set_t t; CPU_ZERO(&t); CPU_SET(1,&t); ],
	        ac_cv_cpu_set_defined=yes,ac_cv_cpu_set_defined=no)])
        if test "$ac_cv_cpu_set_defined" = "yes" ; then
        	AC_DEFINE(HAVE_CPU_SET_MACROS,1,[Define if CPU_SET and CPU_ZERO defined])
        fi
	CFLAGS="$saved_CFLAGS"
	fi
])

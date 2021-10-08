# AX_FUNC_ALIGNED_ALLOC
# ---------------------
#
# Check for the function aligned_alloc()
#
AC_DEFUN([AX_FUNC_ALIGNED_ALLOC],[
saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Werror"
    AC_CACHE_CHECK([checking for aligned_alloc],
                   [ax_cv_func_aligned_alloc],
                   [AC_LINK_IFELSE([AC_LANG_PROGRAM([
			    									 #define _ISOC11_SOURCE
                                                     #include <stdlib.h>
                                                    ],
                                                    [
                                                       aligned_alloc(0,0);
                                                    ])],
                                                    [ax_cv_func_aligned_alloc=yes],
                                                    [ax_cv_func_aligned_alloc=no])])

  if test "x${ax_cv_func_aligned_alloc}" = "xyes" ; then
    AC_DEFINE([HAVE_ALIGNED_ALLOC], [1], [Define to 1 if you have the aligned_alloc() function.])
  fi
CFLAGS="$saved_CFLAGS"
])# AX_ALIGNED_ALLOC

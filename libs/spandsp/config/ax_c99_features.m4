# @synopsis AX_C99_FLEXIBLE_ARRAY
#
# Does the compiler support the 1999 ISO C Standard "struct hack".
# @version 1.1    Mar 15 2004
# @author Erik de Castro Lopo <erikd AT mega-nerd DOT com>
#
# Permission to use, copy, modify, distribute, and sell this file for any 
# purpose is hereby granted without fee, provided that the above copyright 
# and this permission notice appear in all copies.  No representations are
# made about the suitability of this software for any purpose.  It is 
# provided "as is" without express or implied warranty.

AC_DEFUN([AX_C99_FLEXIBLE_ARRAY],
[AC_CACHE_CHECK(C99 struct flexible array support, 
    ac_cv_c99_flexible_array,

# Initialize to unknown
ac_cv_c99_flexible_array=no

AC_TRY_LINK([[
    #include <stdlib.h>

    typedef struct {
    int k;
    char buffer [] ;
    } MY_STRUCT ;
    ]], 
    [  MY_STRUCT *p = calloc (1, sizeof (MY_STRUCT) + 42); ],
    ac_cv_c99_flexible_array=yes,
    ac_cv_c99_flexible_array=no
    ))]
) # AX_C99_FLEXIBLE_ARRAY

# @synopsis AX_C99_FUNC_LRINT
#
# Check whether C99's lrint function is available.
# @version 1.3    Feb 12 2002
# @author Erik de Castro Lopo <erikd AT mega-nerd DOT com>
#
# Permission to use, copy, modify, distribute, and sell this file for any 
# purpose is hereby granted without fee, provided that the above copyright 
# and this permission notice appear in all copies.  No representations are
# made about the suitability of this software for any purpose.  It is 
# provided "as is" without express or implied warranty.
#
AC_DEFUN([AX_C99_FUNC_LRINT],
[AC_CACHE_CHECK(for lrint,
  ac_cv_c99_lrint,
[
lrint_save_CFLAGS=$CFLAGS
CFLAGS="-lm"
AC_TRY_LINK([
#define _ISOC9X_SOURCE  1
#define _ISOC99_SOURCE  1
#define __USE_ISOC99    1
#define __USE_ISOC9X    1

#include <math.h>
], if (!lrint(3.14159)) lrint(2.7183);, ac_cv_c99_lrint=yes, ac_cv_c99_lrint=no)

CFLAGS=$lrint_save_CFLAGS

])

if test "$ac_cv_c99_lrint" = yes; then
  AC_DEFINE(HAVE_LRINT, 1,
            [Define if you have C99's lrint function.])
fi
])# AX_C99_FUNC_LRINT

# @synopsis AX_C99_FUNC_LRINTF
#
# Check whether C99's lrintf function is available.
# @version 1.3    Feb 12 2002
# @author Erik de Castro Lopo <erikd AT mega-nerd DOT com>
#
# Permission to use, copy, modify, distribute, and sell this file for any 
# purpose is hereby granted without fee, provided that the above copyright 
# and this permission notice appear in all copies.  No representations are
# made about the suitability of this software for any purpose.  It is 
# provided "as is" without express or implied warranty.
#
AC_DEFUN([AX_C99_FUNC_LRINTF],
[AC_CACHE_CHECK(for lrintf,
  ac_cv_c99_lrintf,
[
lrintf_save_CFLAGS=$CFLAGS
CFLAGS="-lm"
AC_TRY_LINK([
#define _ISOC9X_SOURCE  1
#define _ISOC99_SOURCE  1
#define __USE_ISOC99    1
#define __USE_ISOC9X    1

#include <math.h>
], if (!lrintf(3.14159)) lrintf(2.7183);, ac_cv_c99_lrintf=yes, ac_cv_c99_lrintf=no)

CFLAGS=$lrintf_save_CFLAGS

])

if test "$ac_cv_c99_lrintf" = yes; then
  AC_DEFINE(HAVE_LRINTF, 1,
            [Define if you have C99's lrintf function.])
fi
])# AX_C99_FUNC_LRINTF

# @synopsis AX_C99_FUNC_LLRINT
#
# Check whether C99's llrint function is available.
# @version 1.1    Sep 30 2002
# @author Erik de Castro Lopo <erikd AT mega-nerd DOT com>
#
# Permission to use, copy, modify, distribute, and sell this file for any 
# purpose is hereby granted without fee, provided that the above copyright 
# and this permission notice appear in all copies.  No representations are
# made about the suitability of this software for any purpose.  It is 
# provided "as is" without express or implied warranty.
#
AC_DEFUN([AX_C99_FUNC_LLRINT],
[AC_CACHE_CHECK(for llrint,
  ac_cv_c99_llrint,
[
llrint_save_CFLAGS=$CFLAGS
CFLAGS="-lm"
AC_TRY_LINK([
#define ISOC9X_SOURCE   1
#define _ISOC99_SOURCE  1
#define __USE_ISOC99    1
#define __USE_ISOC9X    1

#include <math.h>
], long long int x ; x = llrint(3.14159) ;, ac_cv_c99_llrint=yes, ac_cv_c99_llrint=no)

CFLAGS=$llrint_save_CFLAGS

])

if test "$ac_cv_c99_llrint" = yes; then
  AC_DEFINE(HAVE_LLRINT, 1,
            [Define if you have C99's llrint function.])
fi
])# AX_C99_FUNC_LLRINT


# @synopsis AX_C99_FUNC_LLRINTF
#
# Check whether C99's llrintf function is available.
# @version 1.1    Sep 30 2002
# @author Erik de Castro Lopo <erikd AT mega-nerd DOT com>
#
# Permission to use, copy, modify, distribute, and sell this file for any 
# purpose is hereby granted without fee, provided that the above copyright 
# and this permission notice appear in all copies.  No representations are
# made about the suitability of this software for any purpose.  It is 
# provided "as is" without express or implied warranty.
#
AC_DEFUN([AX_C99_FUNC_LLRINTF],
[AC_CACHE_CHECK(for llrintf,
  ac_cv_c99_llrintf,
[
llrintf_save_CFLAGS=$CFLAGS
CFLAGS="-lm"
AC_TRY_LINK([
#define _ISOC9X_SOURCE  1
#define _ISOC99_SOURCE  1
#define __USE_ISOC99    1
#define __USE_ISOC9X    1

#include <math.h>
], long long int x ; x = llrintf(3.14159) ;, ac_cv_c99_llrintf=yes, ac_cv_c99_llrintf=no)

CFLAGS=$llrintf_save_CFLAGS

])

if test "$ac_cv_c99_llrintf" = yes; then
  AC_DEFINE(HAVE_LLRINTF, 1,
            [Define if you have C99's llrintf function.])
fi
])# AX_C99_FUNC_LLRINTF

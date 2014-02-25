dnl @synopsis MN_C_COMPILER_IS_CLANG
dnl
dnl Find out if a compiler claiming to be gcc really is gcc (fuck you clang).
dnl @version 1.0	Oct 31 2013
dnl @author Erik de Castro Lopo <erikd AT mega-nerd DOT com>
dnl
dnl Permission to use, copy, modify, distribute, and sell this file for any
dnl purpose is hereby granted without fee, provided that the above copyright
dnl and this permission notice appear in all copies.  No representations are
dnl made about the suitability of this software for any purpose.  It is
dnl provided "as is" without express or implied warranty.
dnl


AC_DEFUN([MN_C_COMPILER_IS_CLANG],
[AC_CACHE_CHECK(whether we are using the CLANG C compiler,
	mn_cv_c_compiler_clang,
	[	AC_LANG_ASSERT(C)
		AC_TRY_LINK([
			#include <stdio.h>
			],
			[
			#ifndef __clang__
				This is not clang!
			#endif
			],
		mn_cv_c_compiler_clang=yes,
		mn_cv_c_compiler_clang=no
		])
	)
])

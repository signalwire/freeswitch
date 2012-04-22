# @synopsis AX_CHECK_EXPORT_CAPABILITY
#
# Does the compiler support the exporting of library symbols?
# @version 1.0    Jan 31 2009
# @author Steve Underwood
#
# Permission to use, copy, modify, distribute, and sell this file for any 
# purpose is hereby granted without fee, provided that the above copyright 
# and this permission notice appear in all copies.  No representations are
# made about the suitability of this software for any purpose.  It is 
# provided "as is" without express or implied warranty.

AC_DEFUN([AX_CHECK_EXPORT_CAPABILITY],
[AC_CACHE_CHECK([if $1 supports library symbol export], 
    ac_cv_symbol_export_capability,

[# Initialize to unknown
ac_cv_symbol_export_capability="no"

case "${ax_cv_c_compiler_vendor}" in
gnu)
    save_CFLAGS="${CFLAGS}"
    CFLAGS="${CFLAGS} -fvisibility=hidden"
    AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
            [int foo __attribute__ ((visibility("default")));],
            [;]
        )],

        [AC_MSG_RESULT([yes])
         COMP_VENDOR_CFLAGS="-fvisibility=hidden -DHAVE_VISIBILITY=1 $COMP_VENDOR_CFLAGS"
         COMP_VENDOR_CXXFLAGS="-fvisibility=hidden -DHAVE_VISIBILITY=1 $COMP_VENDOR_CXXFLAGS"
         ac_cv_symbol_export_capability="yes"],

        [AC_MSG_RESULT([no])]
    )
    CFLAGS="${save_CFLAGS}"
    ;;

sun)
    save_CFLAGS="${CFLAGS}"
    CFLAGS="${CFLAGS} -xldscope=hidden"
    AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
            [int foo __attribute__ ((visibility("default")));],
            [;]
        )],

        [AC_MSG_RESULT([yes])
         COMP_VENDOR_CFLAGS="-xldscope=hidden -DHAVE_VISIBILITY=1 $COMP_VENDOR_CFLAGS"
         COMP_VENDOR_CXXFLAGS="-xldscope=hidden -DHAVE_VISIBILITY=1 $COMP_VENDOR_CXXFLAGS"
         ac_cv_symbol_export_capability="yes"],

        [AC_MSG_RESULT([no])]
    )
    CFLAGS="${save_CFLAGS}"
    ;;

esac])
AS_IF([test AS_VAR_GET(ac_cv_symbol_export_capability) = yes], [$2], [$3])[]dnl
]) # AX_CHECK_EXPORT_CAPABILITY

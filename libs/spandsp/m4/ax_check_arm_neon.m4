# @synopsis AX_CHECK_ARM_NEON
#
# Does the machine support the ARM NEON instruction set?
# @version 1.01   Feb 11 2013
# @author Steve Underwood
#
# Permission to use, copy, modify, distribute, and sell this file for any 
# purpose is hereby granted without fee, provided that the above copyright 
# and this permission notice appear in all copies.  No representations are
# made about the suitability of this software for any purpose.  It is 
# provided "as is" without express or implied warranty.

AC_DEFUN([AX_CHECK_ARM_NEON],
[AC_CACHE_CHECK([if $1 supports the ARM NEON instructions set], 
    ac_cv_symbol_arm_neon,

[# Initialize to unknown
ac_cv_symbol_arm_neon="no"

case "${ax_cv_c_compiler_vendor}" in
gnu)
    save_CFLAGS="${CFLAGS}"
    CFLAGS="${CFLAGS} -mfpu=neon"
    AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
            [
                #include <inttypes.h>
                #include <arm_neon.h>

                int32x4_t testfunc(int16_t *a, int16_t *b)
                {
                    return vmull_s16(vld1_s16(a), vld1_s16(b));
                }
            ],
            [
                int32x4_t z;
                int16_t x[[8]];
                int16_t y[[8]];
                z = testfunc(x, y);
            ]
        )],

        [AC_MSG_RESULT([yes])
         COMP_VENDOR_CFLAGS="-mfpu=neon $COMP_VENDOR_CFLAGS"
         COMP_VENDOR_CXXFLAGS="-mfpu=neon $COMP_VENDOR_CXXFLAGS"
         ac_cv_symbol_arm_neon="yes"],

        [AC_MSG_RESULT([no])]
    )
    CFLAGS="${save_CFLAGS}"
    ;;

esac])
AS_IF([test AS_VAR_GET(ac_cv_symbol_arm_neon) = yes], [$2], [$3])[]dnl
]) # AX_CHECK_ARM_NEON


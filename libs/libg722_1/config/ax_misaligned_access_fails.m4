# AX_MISALIGNED_ACCESS_FAILS(MACHINE, [ACTION-IF-MISALIGNED-FAILS], [ACTION-IF-MISALIGNED-OK])
# -------------------------------------------------------------------------------------
#
# Check if a specified machine type cannot handle misaligned data. That is, multi-byte data
# types which are not properly aligned in memory fail. Many machines are happy to work with
# misaligned data, but slowing down a bit. Other machines just won't tolerate such data.
#
# This is a simple lookup amongst machines known to the current autotools. So far we only deal
# with the ARM and sparc.
# A lookup is used, as many of the devices which cannot handled misaligned access are embedded
# processors, for which the code normally be cross-compiled. 
#
AC_DEFUN([AX_MISALIGNED_ACCESS_FAILS],
[AS_VAR_PUSHDEF([ac_MisalignedAccessFails], [ac_cv_misaligned_access_fails_$1])dnl
AC_CACHE_CHECK([if $1 fails on misaligned memory access], ac_MisalignedAccessFails,
[case $1 in
      arc | arm | arm[bl]e | arme[lb] | armv[2345] | armv[345][lb] \
    | bfin \
    | sparc \
    | xscale | xscalee[bl] \
    | arm-*  | armbe-* | armle-* | armeb-* | armv*-* \
    | bfin-* \
    | sparc-* \
    | xscale-* | xscalee[bl]-* )
        AS_VAR_SET(ac_MisalignedAccessFails, yes)
        ;;
    *)
        AS_VAR_SET(ac_MisalignedAccessFails, no)
        ;;
esac])
AS_IF([test AS_VAR_GET(ac_MisalignedAccessFails) = yes], [$2], [$3])[]dnl
AS_VAR_POPDEF([ac_MisalignedAccessFails])dnl
])# MISALIGNED_ACCESS_FAILS

# AX_FIXED_POINT_MACHINE(MACHINE, [ACTION-IF-FIXED-POINT], [ACTION-IF-NOT-FIXED-POINT])
# -------------------------------------------------------------------------------------
#
# Check if a specified machine type is a fixed point only machine. That is, if it lacks
# fast floating point support.
#
# This is a simple lookup amongst machines known to the current autotools. So far we deal
# with the embedded ARM, Blackfin, MIPS, TI DSP and XScale processors as things which lack
# fast hardware floating point.
#
# Other candidates would be the small embedded Power PCs.
#
AC_DEFUN([AX_FIXED_POINT_MACHINE],
[AS_VAR_PUSHDEF([ac_FixedPoint], [ac_cv_fixed_point_machine_$1])dnl
AC_CACHE_CHECK([if $1 is fixed point only], ac_FixedPoint,
[case $1 in
      arc \
    | arm | arm[bl]e | arme[bl] | armv[2345] | armv[345][bl] \
    | arm-*  | arm[bl]e-* | arme[bl]-* | armv[345]-* \
    | bfin | bfin-* \
    | mips | mipsbe | mipseb | mipsel | mipsle \
    | mips-* | mipsbe-* | mipseb-* | mipsel-* | mipsle-* \
    | tic54x | c54x* | tic55x | c55x* | tic6x | c6x* \
    | tic30-* | tic4x-* | tic54x-* | tic55x-* | tic6x-* | tic80-* \
    | xscale | xscalee[bl] \
    | xscale-* | xscalee[bl]-* )
        AS_VAR_SET(ac_FixedPoint, yes)
        ;;
    *)
        AS_VAR_SET(ac_FixedPoint, no)
        ;;
esac])
AS_IF([test AS_VAR_GET(ac_FixedPoint) = yes], [$2], [$3])[]dnl
AS_VAR_POPDEF([ac_FixedPoint])dnl
])# AX_FIXED_POINT_MACHINE

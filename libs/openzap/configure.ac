#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([openzap],[pre-alpha],[bugs@freeswitch.prg])
AC_CONFIG_SRCDIR([src/libteletone_detect.c])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Check for com;iler type
AC_DEFUN([AX_COMPILER_VENDOR],
[
AC_CACHE_CHECK([for _AC_LANG compiler vendor], ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor,
 [ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor=unknown
  # note: don't check for GCC first, since some other compilers define __GNUC__
  for ventest in intel:__ICC,__ECC,__INTEL_COMPILER ibm:__xlc__,__xlC__,__IBMC__,__IBMCPP__ gnu:__GNUC__ sun:__SUNPRO_C,__SUNPRO_CC hp:__HP_cc,__HP_aCC dec:__DECC,__DECCXX,__DECC_VER,__DECCXX_VER borland:__BORLANDC__,__TURBOC__ comeau:__COMO__ cray:_CRAYC kai:__KCC lcc:__LCC__ metrowerks:__MWERKS__ sgi:__sgi,sgi microsoft:_MSC_VER watcom:__WATCOMC__ portland:__PGI; do
	vencpp="defined("`echo $ventest | cut -d: -f2 | sed 's/,/) || defined(/g'`")"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[
#if !($vencpp)
      thisisanerror;
#endif
])], [ax_cv_]_AC_LANG_ABBREV[_compiler_vendor=`echo $ventest | cut -d: -f1`; break])
  done
 ])
])
AC_ARG_ENABLE(enable_64,  [  --enable-64 Enable 64bit compilation])

AX_COMPILER_VENDOR

case "${ax_cv_c_compiler_vendor}" in
gnu)
    COMP_VENDOR_CFLAGS="-Wall -Werror -Wextra -Wno-unused-parameter"
    ;;
sun)
    COMP_VENDOR_CFLAGS="-xc99=all -mt -xCC -D__FUNCTION__=__func__ -xvpara"
    if test "$enable_64" = "yes" ; then
        COMP_VENDOR_CFLAGS="-m64 $COMP_VENDOR_CFLAGS"
    fi
    ;;
*)
    COMP_VENDOR_CFLAGS="-Wall -Werror -Wextra -Wno-unused-parameter"
    ;;
esac

COMP_VENDOR_CFLAGS="$COMP_VENDOR_CFLAGS"
AC_SUBST(COMP_VENDOR_CFLAGS)
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

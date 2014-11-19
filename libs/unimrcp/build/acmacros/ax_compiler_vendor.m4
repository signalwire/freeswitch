dnl Determine the vendor of the C/C++ compiler, e.g., gnu, intel, ibm, sun,
dnl hp, borland, comeau, dec, cray, kai, lcc, metrowerks, sgi, microsoft,
dnl watcom, etc. The vendor is returned in the cache variable
dnl $ax_cv_c_compiler_vendor for C and $ax_cv_cxx_compiler_vendor for C++.

AC_DEFUN([AX_COMPILER_VENDOR],
[AC_CACHE_CHECK([for _AC_LANG compiler vendor], ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor,
  [dnl note: don't check for gcc first since some other compilers define __GNUC__
  vendors="intel:     __ICC,__ECC,__INTEL_COMPILER
           ibm:       __xlc__,__xlC__,__IBMC__,__IBMCPP__
           pathscale: __PATHCC__,__PATHSCALE__
           clang:     __clang__
           gnu:       __GNUC__
           sun:       __SUNPRO_C,__SUNPRO_CC
           hp:        __HP_cc,__HP_aCC
           dec:       __DECC,__DECCXX,__DECC_VER,__DECCXX_VER
           borland:   __BORLANDC__,__TURBOC__
           comeau:    __COMO__
           cray:      _CRAYC
           kai:       __KCC
           lcc:       __LCC__
           sgi:       __sgi,sgi
           microsoft: _MSC_VER
           metrowerks: __MWERKS__
           watcom:    __WATCOMC__
           portland:  __PGI
           unknown:   UNKNOWN"
  for ventest in $vendors; do
    case $ventest in
      *:) vendor=$ventest; continue ;;
      *)  vencpp="defined("`echo $ventest | sed 's/,/) || defined(/g'`")" ;;
    esac
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[
      #if !($vencpp)
        thisisanerror;
      #endif
    ])], [break])
  done
  ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor=`echo $vendor | cut -d: -f1`
 ])
])

#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT(libSKP_SILK_SDK, 1.0.8, brian@freeswitch.org, libSKP_SILK_SDK)
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([float.h stdint.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_LIB([m],[pow])

# Absolute source/build directory
abs_srcdir=`(cd $srcdir && pwd)`
abs_builddir=`pwd`
AC_SUBST(abs_srcdir)
AC_SUBST(abs_builddir)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

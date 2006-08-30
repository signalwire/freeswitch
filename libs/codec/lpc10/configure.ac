#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(liblpc10, 1.0, liblpc10)
AM_INIT_AUTOMAKE(liblpc10,1.0)
AC_CONFIG_SRCDIR([f2clib.c])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_INSTALL

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([malloc.h stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([floor sqrt])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AC_INIT([freeswitch_standalone_module], [1.7.0], bugs@freeswitch.org)

AM_INIT_AUTOMAKE

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_PROG_CC_C_O

AC_PROG_LIBTOOL

# backwards compat with older pkg-config
# - pull in AC_DEFUN from pkg.m4
m4_ifndef([PKG_CHECK_VAR], [
# PKG_CHECK_VAR(VARIABLE, MODULE, CONFIG-VARIABLE,
# [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# -------------------------------------------
# Retrieves the value of the pkg-config variable for the given module.
AC_DEFUN([PKG_CHECK_VAR],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_ARG_VAR([$1], [value of $3 for $2, overriding pkg-config])dnl

_PKG_CONFIG([$1], [variable="][$3]["], [$2])
AS_VAR_COPY([$1], [pkg_cv_][$1])

AS_VAR_IF([$1], [""], [$5], [$4])dnl
])# PKG_CHECK_VAR
])

AC_DEFUN([PKG_CHECK_VER],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_ARG_VAR([$1], [version for $2, overriding pkg-config])dnl

_PKG_CONFIG([$1], [modversion], [$2])
AS_VAR_COPY([$1], [pkg_cv_][$1])

AS_VAR_IF([$1], [""], [$4], [$3])dnl
])# PKG_CHECK_VAR

PKG_CHECK_MODULES([FREESWITCH],[freeswitch],[],[])
PKG_CHECK_VAR([moddir],[freeswitch],[modulesdir])
PKG_CHECK_VAR([confdir],[freeswitch],[confdir])
PKG_CHECK_VER([version],[freeswitch])

SWITCH_VERSION_MAJOR=`echo $version|cut -d. -f1`
SWITCH_VERSION_MINOR=`echo $version|cut -d. -f2`
SWITCH_VERSION_MICRO=`echo $version|cut -d. -f3`

AC_SUBST(SWITCH_VERSION_MAJOR, [$SWITCH_VERSION_MAJOR])
AC_SUBST(SWITCH_VERSION_MINOR, [$SWITCH_VERSION_MINOR])
AC_SUBST(SWITCH_VERSION_MICRO, [$SWITCH_VERSION_MICRO])
AC_SUBST(FREESWITCH_CFLAGS)
AC_SUBST(FREESWITCH_LDFLAGS)
AC_SUBST(moddir)
AC_SUBST(confdir)

AC_CONFIG_FILES([Makefile])

AC_OUTPUT

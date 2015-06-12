AC_INIT([freeswitch_standalone_module], [1.7.0], bugs@freeswitch.org)

AM_INIT_AUTOMAKE

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_PROG_CC_C_O

AC_PROG_LIBTOOL

PKG_CHECK_MODULES([FREESWITCH],[freeswitch],[],[])
PKG_CHECK_VAR([moddir],[freeswitch],[modulesdir])

AC_SUBST(FREESWITCH_CFLAGS)
AC_SUBST(FREESWITCH_LDFLAGS)
AC_SUBST(moddir)

AC_CONFIG_FILES([Makefile])

AC_OUTPUT

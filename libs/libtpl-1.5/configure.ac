AC_PREREQ(2.59)

AC_INIT([libtpl], [1.4], [thanson@users.sourceforge.net])
AC_CONFIG_SRCDIR(src/tpl.c)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADERS(config/config.h)
AM_INIT_AUTOMAKE
AC_PROG_CC
dnl next 4 lines are a hack to avoid libtool's
dnl needless checks for C++ and Fortran compilers
m4_undefine([AC_PROG_CXX])
m4_defun([AC_PROG_CXX],[])
m4_undefine([AC_PROG_F77])
m4_defun([AC_PROG_F77],[])
AC_PROG_LIBTOOL

dnl detect Cygwin or MinGW and use mmap family replacements
AC_CONFIG_LIBOBJ_DIR(src/win)
case $host in
     *-*-mingw32* | *-*-cygwin* | *-*-windows*)
     AC_LIBOBJ(mmap)
     AC_MSG_NOTICE([using custom mmap for Cygwin/MinGW])
     ;;
esac

AC_CONFIG_FILES(src/win/Makefile src/Makefile Makefile)
AC_OUTPUT


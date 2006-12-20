dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_PREREQ(2.50)
AC_CONFIG_SRCDIR([configure.ac])
AM_INIT_AUTOMAKE(iksemel,1.2)
AM_CONFIG_HEADER(include/config.h)

AC_CANONICAL_HOST

dnl Checks for programs
AC_PROG_CC
AC_PROG_INSTALL
AC_CHECK_TOOL(OBJDUMP, objdump, :)
AM_PROG_LIBTOOL
AC_LANG_C

dnl Checks for header files
AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h strings.h errno.h)

dnl Checks for typedefs and structures
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])

dnl Special check for windows
case "$host_os" in
	*mingw*)
		AC_CHECK_LIB(wsock32, main, , AC_MSG_ERROR(Winsock is needed for a Win32 build.))
		;;
esac

dnl Check for posix io
AC_ARG_ENABLE(default-io,
[  --disable-default-io    disable builtin socket transport code ],
        defio=$enableval, defio=yes)
if test "$defio" = yes; then
	AC_DEFINE(USE_DEFAULT_IO,1,[Define this is you want default transport])
fi
AM_CONDITIONAL(DO_POSIX, test "x$defio" = "xyes")

dnl Checks for library functions
AC_SEARCH_LIBS(recv,socket)
AC_CHECK_FUNCS(getopt_long)
AC_CHECK_FUNCS(getaddrinfo)

AM_PATH_LIBGNUTLS(,AC_DEFINE(HAVE_GNUTLS,,"Use libgnutls"))

dnl Check -Wall flag of GCC
if test "x$GCC" = "xyes"; then
  if test -z "`echo "$CFLAGS" | grep "\-Wall" 2> /dev/null`" ; then
    CFLAGS="$CFLAGS -Wall"
  fi
fi

dnl Generating makefiles
AC_CONFIG_FILES([
Makefile
iksemel.pc
src/Makefile
include/Makefile
tools/Makefile
test/Makefile
doc/Makefile
])
AC_OUTPUT

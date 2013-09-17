#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT(libedit, [EL_RELEASE],, libedit-[EL_TIMESTAMP_CVSEXPORT])
AC_CONFIG_SRCDIR([src/strlcat.c])
AC_CONFIG_HEADERS([config.h])

CFLAGS="$CFLAGS $CONFIGURE_CFLAGS"
CXXFLAGS="$CXXFLAGS $CONFIGURE_CXXFLAGS"
LDFLAGS="$LDFLAGS $CONFIGURE_LDFLAGS"

AM_INIT_AUTOMAKE
AC_PROG_LIBTOOL

# libtool -version-info
AC_SUBST(LT_VERSION, [0:26:0])

# AC_PROG_LIBTOOL runs AC_CANONICAL_HOST
case $host in 
   *-sun-*) AC_DEFINE([_SUNOS], [], [Solaris's term.h does horrid things.]);;
esac

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LN_S
AC_PROG_AWK
EL_MANTYPE

AC_CHECK_LIB(tinfo, tgetent, [have_libtinfo="yes"], [have_libtinfo="no"])
if test "$have_libtinfo" == "yes" ; then
   TINFO_LIBS="-ltinfo"
else
   TINFO_LIBS=""
   AC_CHECK_LIB(ncurses, tgetent,,
      [AC_CHECK_LIB(curses, tgetent,,
         [AC_MSG_ERROR([libtermcap, libcurses or libncurses are required!])] )] )
fi
AC_SUBST(TINFO_LIBS)

# Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h limits.h malloc.h stdlib.h string.h sys/ioctl.h sys/param.h unistd.h curses.h ncurses.h sys/cdefs.h])

AC_CHECK_HEADER([termios.h], [], [AC_MSG_ERROR([termios.h is required!])],[])

## include curses.h to prevent "Present But Cannot Be Compiled"
AC_CHECK_HEADERS([term.h],,,
[[#if HAVE_CURSES_H
# include <curses.h>
#elif HAVE_NCURSES_H
# include <ncurses.h>
#endif
]])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_TYPES([u_int32_t])

# Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
## _AIX is offended by rpl_malloc and rpl_realloc
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_CHECK_FUNCS([endpwent isascii memchr memset re_comp regcomp strcasecmp strchr strcspn strdup strerror strrchr strstr strtol issetugid])
EL_GETPW_R_POSIX
EL_GETPW_R_DRAFT

AH_BOTTOM([
#include <sys.h>
#define SCCSID
#undef LIBC_SCCS
])

AC_CONFIG_FILES([Makefile
                 libedit.pc
                 src/Makefile
                 doc/Makefile
                 examples/Makefile])
AC_OUTPUT

dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_PREREQ(2.50)
AC_CONFIG_AUX_DIR(build)
AC_CONFIG_SRCDIR([configure.ac])
AM_INIT_AUTOMAKE(iksemel,1.3)
AC_CONFIG_HEADERS(include/config.h)

CFLAGS="$CFLAGS $CONFIGURE_CFLAGS"
CXXFLAGS="$CXXFLAGS $CONFIGURE_CXXFLAGS"
LDFLAGS="$LDFLAGS $CONFIGURE_LDFLAGS"

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

AX_PATH_LIBGNUTLS(,AC_DEFINE(HAVE_GNUTLS,,"Use libgnutls"))

dnl Check -Wall flag of GCC
if test "x$GCC" = "xyes"; then
  if test -z "`echo "$CFLAGS" | grep "\-Wall" 2> /dev/null`" ; then
    CFLAGS="$CFLAGS -Wall"
  fi
fi

AC_DEFUN([AX_COMPILER_VENDOR],
[
AC_CACHE_CHECK([for _AC_LANG compiler vendor], ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor,
 [ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor=unknown
  # note: don't check for gcc first since some other compilers define __GNUC__
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

AX_COMPILER_VENDOR

#  Enable 64 bit build
AC_ARG_ENABLE(64,
[AC_HELP_STRING([--enable-64],[build with 64 bit support])],[enable_64="$enable_64"],[enable_64="no"])

if test "x${ax_cv_c_compiler_vendor}"  =  "xsun" ; then
    if test "${enable_64}" = "yes"; then
        CFLAGS="$CFLAGS -m64"
        CXXFLAGS="$CXXFLAGS -m64"
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

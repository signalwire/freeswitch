##
dnl ##  OSSP js - JavaScript Engine
dnl ##  Copyright (c) 1998-2006 Mozilla <http://www.mozilla.org/>
dnl ##
dnl ##  This file is part of OSSP js, a distribution of the Mozilla JavaScript
dnl ##  reference implementation, which can found at http://www.ossp.org/pkg/lib/js/
dnl ##
dnl ##  Permission to use, copy, modify, and distribute this software for
dnl ##  any purpose with or without fee is hereby granted, provided that
dnl ##  the above copyright notice and this permission notice appear in all
dnl ##  copies.
dnl ##
dnl ##  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
dnl ##  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
dnl ##  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
dnl ##  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
dnl ##  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
dnl ##  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
dnl ##  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
dnl ##  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
dnl ##  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
dnl ##  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
dnl ##  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
dnl ##  SUCH DAMAGE.
dnl ##
dnl ##  configure.ac: GNU Autoconf source script
dnl ##

AC_PREREQ(2.59)
AC_INIT
JS_VERSION=`./shtool version -l txt -d short VERSION`
$ac_confdir/shtool echo -e \
    "Configuring %BOSSP js%b (Mozilla JavaScript Engine), version %B${JS_VERSION}%b"
AC_SUBST(JS_VERSION)

CFLAGS="$CFLAGS $CONFIGURE_CFLAGS"
CXXFLAGS="$CXXFLAGS $CONFIGURE_CXXFLAGS"
LDFLAGS="$LDFLAGS $CONFIGURE_LDFLAGS"

AC_CANONICAL_BUILD
AC_CANONICAL_HOST

AC_PROG_MAKE_SET
AC_PROG_CC
AC_CHECK_DEBUGGING
AC_CHECK_VA_COPY
AC_PROG_LIBTOOL

OLD_CFLAGS="$CFLAGS"
CFLAGS=""
AC_CHECK_LIB(nsl, gethostname)
if test ".`echo $LIBS | grep nsl`" = .; then
    AC_CHECK_LIB(nsl, gethostbyname)
fi
AC_CHECK_LIB(socket, accept)
AC_CHECK_LIB(m, floor)
CFLAGS="$OLD_CFLAGS"

AC_CHECK_MEMBERS([struct stat.st_birthtime, struct stat.st_birthtimensec],,, [#include <sys/stat.h>])

CPPFLAGS="$CPPFLAGS -DOSSP"
case "${host}" in
    *mingw* | *windows* | *winnt* ) CPPFLAGS="$CPPFLAGS -DXP_WIN"  ;;
    *                             ) CPPFLAGS="$CPPFLAGS -DXP_UNIX" ;;
esac
CPPFLAGS="$CPPFLAGS -DEXPORT_JS_API"

dnl #   configure option --with-version
AC_ARG_WITH([version],
    AC_HELP_STRING([--with-version], [build the run-time engine with JavaScript features only. Available versions are: ECMA-3, JS-1.5, JS-1.6 (default)]),
    [ac_cv_with_version=$withval], [ac_cv_with_version=no])
AC_CACHE_CHECK([whether to build the run-time engine with JavaScript features only],
    [ac_cv_with_version], [ac_cv_with_version=no])
case "$ac_cv_with_version" in
    dnl JS-1.0 ) CPPFLAGS="$CPPFLAGS -DJS_VERSION=100" ;;
    dnl JS-1.1 ) CPPFLAGS="$CPPFLAGS -DJS_VERSION=110" ;;
    dnl JS-1.2 ) CPPFLAGS="$CPPFLAGS -DJS_VERSION=120" ;;
    dnl JS-1.3 ) CPPFLAGS="$CPPFLAGS -DJS_VERSION=130" ;;
    dnl JS-1.4 ) CPPFLAGS="$CPPFLAGS -DJS_VERSION=140" ;;
    ECMA-3 ) CPPFLAGS="$CPPFLAGS -DJS_VERSION=148" ;;
    JS-1.5 ) CPPFLAGS="$CPPFLAGS -DJS_VERSION=150" ;;
    JS-1.6 ) CPPFLAGS="$CPPFLAGS -DJS_VERSION=160" ;;
esac

CLI_CPPFLAGS=""
CLI_CFLAGS=""
CLI_LDFLAGS=""
CLI_LIBS=""
CLI_OBJS=""

dnl #   configure option --with-editline[=path]
AC_ARG_WITH([editline],
    AC_HELP_STRING([--with-editline], [build command line interface with line editing support via editline, libedit or GNU readline]),
    [ac_cv_with_editline=$withval], [ac_cv_with_editline=no])
AC_CACHE_CHECK([whether to build command line interface with line editing support],
    [ac_cv_with_editline], [ac_cv_with_editline=no])
if test ".$ac_cv_with_editline" != ".no"; then
    CLI_CPPFLAGS="$CLI_CPPFLAGS -DEDITLINE"
    case "$ac_cv_with_editline" in
        /* ) CLI_LDFLAGS="-L$ac_cv_with_editline" ;;
    esac
    OLD_LDFLAGS="$LDFLAGS";
    OLD_LIBS="$LIBS";
    LDFLAGS="$LDFLAGS $CLI_LDFLAGS"
    LIBS=""
    AC_SEARCH_LIBS(tcgetattr, termcap termlib curses ncurses)
    AC_SEARCH_LIBS(tputs, termcap termlib curses ncurses)
    TERM_LIBS="$LIBS"
    AC_SEARCH_LIBS(readline, editline edit readline,,, [$TERM_LIBS])
    CLI_LIBS="$LIBS"
    LDFLAGS="$OLD_LDFLAGS"
    LIBS="$OLD_LIBS"
fi

dnl #   configure option --with-perl
AC_ARG_WITH([perl],
	AS_HELP_STRING([--with-perl], [build Perl-to-JS and JS-to-Perl bindings]),
	[ac_cv_with_perl=$withval], [ac_cv_with_perl=no])
AC_CACHE_CHECK([whether to build Perl-to-JS and JS-to-Perl bindings], [ac_cv_with_perl], [ac_cv_with_perl=no])
if test ".$ac_cv_with_perl" != ".no"; then
	WITH_PERL="yes"
	CLI_OBJS="$CLI_OBJS src/perlconnect/jsperl.o"
	CLI_CPPFLAGS="$CLI_CPPFLAGS -DPERLCONNECT"
else
	WITH_PERL="no"
fi
AC_SUBST(WITH_PERL)
case "$ac_cv_with_perl" in
    /* ) PERL="$ac_cv_with_perl" ;;
esac
AC_PATH_PROG(PERL, perl, NA)
if test ".$ac_cv_with_perl" = ".yes" -a ".$PERL" = ".NA"; then
	AC_ERROR([required Perl interpreter not found in \$PATH])
fi
if test ".$ac_cv_with_perl" != ".no"; then
    CLI_CFLAGS="$CLI_CFLAGS `$PERL -MExtUtils::Embed -e ccopts`"
    CLI_LIBS="$CLI_LIBS `$PERL -MExtUtils::Embed -e ldopts`"
fi

dnl #   configure option --without-file
AC_ARG_WITH([file],
	AS_HELP_STRING([--without-file], [build without File object (grants access to the filesystem)]),
	[ac_cv_with_file=$withval], [ac_cv_with_file=yes])
AC_CACHE_CHECK([whether to build with the File object], [ac_cv_with_file], [ac_cv_with_file=yes])
if test ".$ac_cv_with_file" = ".yes"; then
    CPPFLAGS="$CPPFLAGS -DJS_HAS_FILE_OBJECT=1"
fi

dnl #   configure option --without-xml
AC_ARG_WITH([xml],
	AS_HELP_STRING([--without-xml], [build without XML object]),
	[ac_cv_with_xml=$withval], [ac_cv_with_xml=yes])
AC_CACHE_CHECK([whether to build with the XML object], [ac_cv_with_xml], [ac_cv_with_xml=yes])
if test ".$ac_cv_with_xml" = ".yes"; then
    CPPFLAGS="$CPPFLAGS -DJS_HAS_XML_SUPPORT=1"
fi

dnl #   configure option --with-dso
AC_ARG_WITH([dso],
	AS_HELP_STRING([--with-dso], [build without DSO object (allows run-time process extending)]),
	[ac_cv_with_dso=$withval], [ac_cv_with_dso=no])
AC_CACHE_CHECK([whether to build with the DSO object], [ac_cv_with_dso], [ac_cv_with_dso=no])
if test ".$ac_cv_with_dso" = ".yes"; then
    AC_CHECK_LIB(dl, dlopen)
    AC_CHECK_HEADER(dlfcn.h, , AC_ERROR([dlopen(3) header <dlfcn.h> required]))
    AC_CHECK_FUNCS(dlopen dlclose dlerror, , AC_ERROR([dlopen(3) API functions dlopen/dlclose/dlerror required]))
    CPPFLAGS="$CPPFLAGS -DJS_HAS_DSO_OBJECT=1"
    CLI_LDFLAGS="$CLI_LDFLAGS -export-dynamic"
fi

dnl #   configure option --with-utf8
AC_ARG_WITH([utf8],
	AS_HELP_STRING([--with-utf8], [build with exclusive UTF-8 C strings (uses Unicode UTF-8 encoded C strings only)]),
	[ac_cv_with_utf8=$withval], [ac_cv_with_utf8=no])
AC_CACHE_CHECK([whether to build with exclusive UTF-8 C string], [ac_cv_with_utf8], [ac_cv_with_utf8=no])
if test ".$ac_cv_with_utf8" = ".yes"; then
    CPPFLAGS="$CPPFLAGS -DJS_C_STRINGS_ARE_UTF8=1"
fi

AC_ARG_WITH(nspr,
	AS_HELP_STRING([--with-nspr], [build with NSPR for thread safety]),
	[ac_cv_with_nspr=$withval], [ac_cv_with_nspr=no])
AC_CACHE_CHECK([whether to build with NSPR], [ac_cv_with_nspr], [ac_cv_with_nspr=no])
if test ".$ac_cv_with_nspr" = ".yes"; then
    sinclude(nsprpub/config/nspr.m4)

    AC_CHECK_LIB(pthread, pthread_create, [CLI_LIBS="$CLI_LIBS -lpthread"])

    CPPFLAGS="$CPPFLAGS -DJS_THREADSAFE=1 -Insprpub/dist/include/nspr -Insprpub/pr/include"
    LDFLAGS="$LDFLAGS  -L./nsprpub/dist/lib"
    LIBS="$LIBS -lnspr4"
    LIBPATH="LD_LIBRARY_PATH=./nsprpub/dist/lib DYLD_LIBRARY_PATH=./nsprpub/dist/lib" 
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

AC_SUBST(CLI_CFLAGS)
AC_SUBST(CLI_CPPFLAGS)
AC_SUBST(CLI_LDFLAGS)
AC_SUBST(CLI_LIBS)
AC_SUBST(CLI_OBJS)
AC_SUBST(LIBPATH)

with_tags=""

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_FILES([Makefile js.pc js-config.sh src/perlconnect/Makefile.PL])
AC_OUTPUT


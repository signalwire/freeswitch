dnl
dnl Process this file with autoconf to produce a configure script
dnl

AC_PREREQ(2.50)
AC_INIT(export_vars.sh.in)

AC_CONFIG_HEADERS(include/private/apu_config.h)
AC_CONFIG_AUX_DIR(build)

sinclude(build/apu-conf.m4)
sinclude(build/apu-iconv.m4)
sinclude(build/apu-hints.m4)
sinclude(build/apr_common.m4)
sinclude(build/find_apr.m4)
sinclude(build/dbm.m4)
sinclude(build/dbd.m4)

dnl Generate ./config.nice for reproducing runs of configure
dnl 
APR_CONFIG_NICE(config.nice)

CFLAGS="$CFLAGS $CONFIGURE_CFLAGS"
CXXFLAGS="$CXXFLAGS $CONFIGURE_CXXFLAGS"
LDFLAGS="$LDFLAGS $CONFIGURE_LDFLAGS"

dnl # Some initial steps for configuration.  We setup the default directory
dnl # and which files are to be configured.

dnl Absolute source/build directory
abs_srcdir=`(cd $srcdir && pwd)`
abs_builddir=`pwd`

if test "$abs_builddir" != "$abs_srcdir"; then
  USE_VPATH=1
  APU_CONFIG_LOCATION=build
else
  APU_CONFIG_LOCATION=source
fi

AC_SUBST(APU_CONFIG_LOCATION)

AC_CANONICAL_SYSTEM

AC_PROG_INSTALL

dnl
dnl compute the top directory of the build
dnl note: this is needed for LIBTOOL and exporting the bundled Expat
dnl
top_builddir="$abs_builddir"
AC_SUBST(top_builddir)
AC_SUBST(abs_srcdir)
AC_SUBST(abs_builddir)

dnl Initialize mkdir -p functionality.
APR_MKDIR_P_CHECK($abs_srcdir/build/mkdir.sh)

dnl get our version information
get_version="$abs_srcdir/build/get-version.sh"
version_hdr="$abs_srcdir/include/apu_version.h"
APRUTIL_MAJOR_VERSION="`$get_version major $version_hdr APU`"
APRUTIL_DOTTED_VERSION="`$get_version all $version_hdr APU`"

AC_SUBST(APRUTIL_DOTTED_VERSION)
AC_SUBST(APRUTIL_MAJOR_VERSION)

echo "APR-util Version: ${APRUTIL_DOTTED_VERSION}"

dnl Enable the layout handling code, then reparse the prefix-style
dnl arguments due to autoconf being a PITA.
APR_ENABLE_LAYOUT(apr-util)
APR_PARSE_ARGUMENTS

dnl load os-specific hints for apr-util
APU_PRELOAD

dnl
dnl set up the compilation flags and stuff
dnl

APRUTIL_INCLUDES=""
APRUTIL_PRIV_INCLUDES="-I$top_builddir/include -I$top_builddir/include/private"
if test -n "$USE_VPATH"; then
    APRUTIL_PRIV_INCLUDES="$APRUTIL_PRIV_INCLUDES -I$abs_srcdir/include/private -I$abs_srcdir/include"
fi

dnl
dnl Find the APR includes directory and (possibly) the source (base) dir.
dnl
APU_FIND_APR

dnl
dnl even though we use apr_rules.mk for building apr-util, we need
dnl to grab CC and CPP ahead of time so that apr-util config tests
dnl use the same compiler as APR; we need the same compiler options
dnl and feature test macros as well
dnl
APR_SETIFNULL(CC, `$apr_config --cc`)
APR_SETIFNULL(CPP, `$apr_config --cpp`)
APR_ADDTO(CFLAGS, `$apr_config --cflags`)
APR_ADDTO(CPPFLAGS, `$apr_config --cppflags`)

dnl
dnl  Find the APR-ICONV directory.
dnl
if test -d ../apr-iconv; then
  APR_SUBDIR_CONFIG(../apr-iconv,
                    [$apache_apr_flags --prefix=$prefix --exec-prefix=$exec_prefix --libdir=$libdir --includedir=$includedir --bindir=$bindir --datadir=$datadir --with-installbuilddir=$installbuilddir],
                    [--enable-layout=*|\'--enable-layout=*])
  APRUTIL_EXPORT_LIBS="$abs_srcdir/../apr-iconv/lib/libapriconv.la $APRUTIL_EXPORT_LIBS"
  APRUTIL_INCLUDES="-I$abs_srcdir/../apr-iconv/include $APRUTIL_INCLUDES"
  APR_ICONV_DIR=../apr-iconv
else
  APR_ICONV_DIR=""
fi
AC_SUBST(APR_ICONV_DIR)

dnl Find LDAP library
dnl Determine what DBM backend type to use.
dnl Find Expat
dnl Find an iconv library
APU_FIND_LDAP
APU_CHECK_DBM
APU_CHECK_DBD
APU_CHECK_DBD_MYSQL
APU_CHECK_DBD_SQLITE3
APU_CHECK_DBD_SQLITE2
APU_FIND_EXPAT
APU_FIND_ICONV

AC_SEARCH_LIBS(crypt, crypt ufc)
AC_MSG_CHECKING(if system crypt() function is threadsafe)
if test "x$apu_crypt_threadsafe" = "x1"; then
  AC_DEFINE(APU_CRYPT_THREADSAFE, 1, [Define if the system crypt() function is threadsafe])
  msg="yes"
else
  msg="no"
fi
AC_MSG_RESULT([$msg])

AC_CHECK_FUNCS(crypt_r, [ crypt_r="1" ], [ crypt_r="0" ])
if test "$crypt_r" = "1"; then
  APU_CHECK_CRYPT_R_STYLE
fi

so_ext=$APR_SO_EXT
lib_target=$APR_LIB_TARGET
AC_SUBST(so_ext)
AC_SUBST(lib_target)

APRUTIL_LIBNAME="aprutil${libsuffix}"
AC_SUBST(APRUTIL_LIBNAME)

dnl
dnl Prep all the flags and stuff for compilation and export to other builds
dnl
APR_ADDTO(APRUTIL_LIBS, [$APR_LIBS])

AC_SUBST(APRUTIL_EXPORT_LIBS)
AC_SUBST(APRUTIL_PRIV_INCLUDES)
AC_SUBST(APRUTIL_INCLUDES)
AC_SUBST(APRUTIL_LDFLAGS)
AC_SUBST(APRUTIL_LIBS)
AC_SUBST(LDFLAGS)

dnl copy apr's rules.mk into our build directory.
if test ! -d ./build; then
   $mkdir_p build
fi
cp $APR_BUILD_DIR/apr_rules.mk $abs_builddir/build/rules.mk

dnl
dnl BSD/OS (BSDi) needs to use a different include syntax in the Makefiles
dnl
case "$host_alias" in
*bsdi* | BSD/OS)
    # Check whether they've installed GNU make
    if make --version > /dev/null 2>&1; then
        INCLUDE_RULES="include $abs_builddir/build/rules.mk"
        INCLUDE_OUTPUTS="include $abs_srcdir/build-outputs.mk"
    else
        INCLUDE_RULES=".include \"$abs_builddir/build/rules.mk\""
        INCLUDE_OUTPUTS=".include \"$abs_srcdir/build-outputs.mk\""
    fi
    ;;
*)
    INCLUDE_RULES="include $abs_builddir/build/rules.mk"
    INCLUDE_OUTPUTS="include $abs_srcdir/build-outputs.mk"
    ;;
esac
AC_SUBST(INCLUDE_RULES)
AC_SUBST(INCLUDE_OUTPUTS)

for d in include include/private; do
    test -d $top_builddir/$d || mkdir $top_builddir/$d
done

AC_CONFIG_FILES([Makefile export_vars.sh
                 build/pkg/pkginfo apr-util.pc
                 apu-$APRUTIL_MAJOR_VERSION-config:apu-config.in
                 include/private/apu_select_dbm.h
                 include/apr_ldap.h
                 include/apu.h include/apu_want.h])

AC_CONFIG_COMMANDS([default], [
chmod +x apu-$APRUTIL_MAJOR_VERSION-config
],[
APRUTIL_MAJOR_VERSION=$APRUTIL_MAJOR_VERSION
])

if test -d $srcdir/test; then
    AC_CONFIG_FILES([test/Makefile])
fi

AC_OUTPUT

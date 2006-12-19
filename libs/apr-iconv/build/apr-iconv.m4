dnl
dnl custom autoconf rules for APRICONV
dnl

dnl
dnl API_FIND_APR: figure out where APR is located
dnl
AC_DEFUN(API_FIND_APR,[

  dnl use the find_apr.m4 script to locate APR. sets apr_found and apr_config
  APR_FIND_APR(,,,[1])
  if test "$apr_found" = "no"; then
    AC_MSG_ERROR(APR could not be located. Please use the --with-apr option.)
  fi

  APR_BUILD_DIR="`$apr_config --installbuilddir`"

  dnl make APR_BUILD_DIR an absolute directory (we'll need it in the
  dnl sub-projects in some cases)
  APR_BUILD_DIR="`cd $APR_BUILD_DIR && pwd`"

  APR_INCLUDES="`$apr_config --includes`"
  APR_LIBS="`$apr_config --link-libtool --libs`"
  APR_SO_EXT="`$apr_config --apr-so-ext`"
  APR_LIB_TARGET="`$apr_config --apr-lib-target`"

  AC_SUBST(APR_INCLUDES)
  AC_SUBST(APR_LIBS)
  AC_SUBST(APR_SO_EXT)
  AC_SUBST(APR_LIB_TARGET)

  dnl ### would be nice to obsolete these
  APR_SOURCE_DIR="`$apr_config --srcdir`"
  AC_SUBST(APR_BUILD_DIR)
  AC_SUBST(APR_SOURCE_DIR)
])

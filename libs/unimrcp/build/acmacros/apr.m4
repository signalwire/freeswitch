dnl  UNIMRCP_CHECK_APR

AC_DEFUN([UNIMRCP_CHECK_APR],
[
  AC_MSG_NOTICE([Apache Portable Runtime (APR) library configuration])

  APR_FIND_APR("", "", 1, 1)

  if test $apr_found = "no"; then
    AC_MSG_WARN([APR not found])
    UNIMRCP_DOWNLOAD_APR
  fi

  if test $apr_found = "reconfig"; then
    AC_MSG_WARN([APR reconfig])
  fi

  dnl check APR version number

  apr_version="`$apr_config --version`"
  AC_MSG_RESULT([$apr_version])

  dnl Get build information from APR

  CPPFLAGS="$CPPFLAGS `$apr_config --cppflags`"
  CFLAGS="$CFLAGS `$apr_config --cflags`"
  LDFLAGS="$LDFLAGS `$apr_config --ldflags`"

  UNIMRCP_APR_INCLUDES="`$apr_config --includes`"
  UNIMRCP_APR_LIBS="`$apr_config --link-libtool --libs`"

  AC_SUBST(UNIMRCP_APR_INCLUDES)
  AC_SUBST(UNIMRCP_APR_LIBS)
])

dnl UNIMRCP_DOWNLOAD_APR
dnl no apr found, print out a message telling the user what to do
AC_DEFUN([UNIMRCP_DOWNLOAD_APR],
[
  echo "The Apache Portable Runtime (APR) library cannot be found."
  echo "Please install APR on this system and supply the appropriate"
  echo "--with-apr option to 'configure'"
  AC_MSG_ERROR([no suitable APR found])
])

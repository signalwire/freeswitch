dnl
dnl UNIMRCP_CHECK_APR
dnl
dnl This macro attempts to find APR and APR-util libraries and
dnl set corresponding variables on exit.
dnl
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
  APR_ADDTO(CPPFLAGS,`$apr_config --cppflags`)
  APR_ADDTO(CFLAGS,`$apr_config --cflags`)
  APR_ADDTO(LDFLAGS,`$apr_config --ldflags`)

  APR_ADDTO(UNIMRCP_APR_INCLUDES,`$apr_config --includes`)
  APR_ADDTO(UNIMRCP_APR_LIBS,`$apr_config --link-ld`)

  AC_MSG_NOTICE([Apache Portable Runtime Utility (APU) library configuration])

  APR_FIND_APU("", "", 1, 1)

  if test $apu_found = "no"; then
    AC_MSG_WARN([APU not found])
    UNIMRCP_DOWNLOAD_APU
  fi

  if test $apu_found = "reconfig"; then
    AC_MSG_WARN([APU reconfig])
  fi

  dnl check APU version number
  apu_version="`$apu_config --version`"
  AC_MSG_RESULT([$apu_version])

  dnl Get build information from APU
  APR_ADDTO(LDFLAGS,`$apu_config --ldflags`)

  APR_ADDTO(UNIMRCP_APR_INCLUDES,`$apu_config --includes`)
  APR_ADDTO(UNIMRCP_APR_LIBS,`$apu_config --link-ld`)

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

dnl UNIMRCP_DOWNLOAD_APU
dnl no apr-util found, print out a message telling the user what to do
AC_DEFUN([UNIMRCP_DOWNLOAD_APU],
[
  echo "The Apache Portable Runtime Utility (APU) library cannot be found."
  echo "Please install APRUTIL on this system and supply the appropriate"
  echo "--with-apr-util option to 'configure'"
  AC_MSG_ERROR([no suitable APU found])
])

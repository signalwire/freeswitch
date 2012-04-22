dnl  UNIMRCP_CHECK_APU

AC_DEFUN([UNIMRCP_CHECK_APU],
[
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

  LDFLAGS="$LDFLAGS `$apu_config --ldflags`"

  UNIMRCP_APU_INCLUDES="`$apu_config --includes`"
  UNIMRCP_APU_LIBS="`$apu_config --link-libtool --libs`"

  AC_SUBST(UNIMRCP_APU_INCLUDES)
  AC_SUBST(UNIMRCP_APU_LIBS)
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

# AX_CHECK_REAL_FILE(FILE, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# ------------------------------------------------------------------
#
# Check for the existence of FILE, and make sure it is a real file or
# directory, and not a symbolic link.
#
AC_DEFUN([AX_CHECK_REAL_FILE],
[AC_DIAGNOSE([cross],
	     [cannot check for file existence when cross compiling])dnl
AS_VAR_PUSHDEF([ac_RealFile], [ac_cv_real_file_$1])dnl
AC_CACHE_CHECK([for $1], ac_RealFile,
[test "$cross_compiling" = yes &&
  AC_MSG_ERROR([cannot check for file existence when cross compiling])
if test -r "$1"
then
  if test -h "$1"
  then
    AS_VAR_SET(ac_RealFile, no)
  else
    AS_VAR_SET(ac_RealFile, yes)
  fi
else
  AS_VAR_SET(ac_RealFile, no)
fi])
AS_IF([test AS_VAR_GET(ac_RealFile) = yes], [$2], [$3])[]dnl
AS_VAR_POPDEF([ac_RealFile])dnl
])# AX_CHECK_REAL_FILE

AC_DEFUN([AC_PROG_GZIP],[
AC_CHECK_PROGS(gzip,[gzip],no)
export gzip;
if test $gzip = "no" ;
then
        AC_MSG_ERROR([Unable to find the gzip application]);
fi
AC_SUBST(gzip)
])

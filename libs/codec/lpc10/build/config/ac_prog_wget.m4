AC_DEFUN([AC_PROG_WGET],[
AC_CHECK_PROGS(wget,[wget],no)
export wget;
if test $wget = "no" ;
then
        AC_MSG_ERROR([Unable to find the wget application]);
fi
AC_SUBST(wget)
])

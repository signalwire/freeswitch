AC_DEFUN([AC_FUNC_MEMMOVE],
[AC_CHECK_FUNCS(memmove)
AC_MSG_CHECKING(for working memmove)
AC_CACHE_VAL(ac_cv_have_working_memmove,
[AC_TRY_RUN(
[#include <stdio.h>

int main(void)
{
    char buf[10];
    strcpy (buf, "01234567");
    memmove (buf, buf + 2, 3);
    if (strcmp (buf, "23434567"))
        exit (1);
    strcpy (buf, "01234567");
    memmove (buf + 2, buf, 3);
    if (strcmp (buf, "01012567"))
        exit (1);
    exit (0);
}], ac_cv_have_working_memmove=yes, ac_cv_have_working_memmove=no, ac_cv_have_working_memmove=cross)])
AC_MSG_RESULT([$ac_cv_have_working_memmove])
if test x$ac_cv_have_working_memmove != "xyes"; then
  AC_LIBOBJ(memmove)
  AC_MSG_WARN([Replacing missing/broken memmove.])
  AC_DEFINE(PREFER_PORTABLE_MEMMOVE, 1, "enable replacement memmove if system memmove is broken or missing")
fi])


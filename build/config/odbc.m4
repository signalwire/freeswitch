dnl Derrick Brashear
dnl from KTH krb and Arla

AC_DEFUN([ODBC_INC_WHERE1], [
ac_cv_found_odbc_inc=no
if test -f "$1/sql.h" ; then
  ac_cv_found_odbc_inc=yes
fi
])

AC_DEFUN([ODBC_INC_WHERE], [
   for i in $1; do
      AC_MSG_CHECKING(for odbc header in $i)
      ODBC_INC_WHERE1($i)
      if test "$ac_cv_found_odbc_inc" = "yes"; then
        ac_cv_odbc_where_inc=$i
        AC_MSG_RESULT(found)
        break
      else
        AC_MSG_RESULT(no found)
      fi
    done
])

AC_DEFUN([ODBC_LIB_WHERE1], [
saved_LIBS=$LIBS
saved_CFLAGS=$CFLAGS
case "$host" in
     *darwin*)
	LIBS="$saved_LIBS -L$1 -lodbc -framework CoreFoundation"
     ;;
     *)
	LIBS="$saved_LIBS -L$1 -lodbc"
     ;;
esac
CFLAGS="$saved_CFLAGS -I$ac_cv_odbc_where_inc"
AC_TRY_LINK(
[#include <sql.h>],
[SQLHDBC con;
SQLDisconnect(con);],
[ac_cv_found_odbc_lib=yes],
ac_cv_found_odbc_lib=no)
LIBS=$saved_LIBS
CFLAGS=$saved_CFLAGS
])

AC_DEFUN([TEST_LIBPATH], [
changequote(<<, >>)
define(<<AC_CV_FOUND>>, translit(ac_cv_found_$2_lib, <<- *>>, <<__p>>))
changequote([, ])
if test "$AC_CV_FOUND" = "yes"; then
  if test \! -r "$1/lib$2.a" -a \! -r "$1/lib$2.so" -a \! -r "$1/lib$2.sl" -a \! -r "$1/lib$2.dylib"; then
    AC_CV_FOUND=no
  fi
fi
])


AC_DEFUN([ODBC_LIB_WHERE], [
   for i in $1; do
      AC_MSG_CHECKING(for odbc library in $i)
      ODBC_LIB_WHERE1($i)
      TEST_LIBPATH($i, odbc)
      if test "$ac_cv_found_odbc_lib" = "yes" ; then
        ac_cv_odbc_where_lib=$i
        AC_MSG_RESULT(found)
        break
      else
        AC_MSG_RESULT(no found)
      fi
    done
])

AC_DEFUN([FIND_LIB_SUBDIR],
[dnl
AC_ARG_WITH([lib-subdir], AC_HELP_STRING([--with-lib-subdir=DIR],[Find libraries in DIR instead of lib]))
AC_CHECK_SIZEOF(long)
AC_CACHE_CHECK([what directory libraries are found in], [ac_cv_cmu_lib_subdir],
[test "X$with_lib_subdir" = "Xyes" && with_lib_subdir=
test "X$with_lib_subdir" = "Xno" && with_lib_subdir=
 if test "X$with_lib_subdir" = "X" ; then
   ac_cv_cmu_lib_subdir=lib
   if test $ac_cv_sizeof_long -eq 4 ; then
     test -d /usr/lib32 && ac_cv_cmu_lib_subdir=lib32
   fi
   if test $ac_cv_sizeof_long -eq 8 ; then
     test -d /usr/lib64 && ac_cv_cmu_lib_subdir=lib64
   fi
 else
   ac_cv_cmu_lib_subdir=$with_lib_subdir
 fi])
 AC_SUBST(LIB_SUBDIR, $ac_cv_cmu_lib_subdir)
 ])
 

AC_DEFUN([AX_LIB_ODBC], [
AC_REQUIRE([FIND_LIB_SUBDIR])
AC_ARG_WITH(odbc,
	[  --with-odbc=PREFIX      Compile with ODBC support],
	[if test "X$with_odbc" = "X"; then
		with_odbc=yes
	fi])
AC_ARG_WITH(odbc-lib,
	[  --with-odbc-lib=dir     use odbc libraries in dir],
	[if test "$withval" = "yes" -o "$withval" = "no"; then
		AC_MSG_ERROR([No argument for --with-odbc-lib])
	fi])
AC_ARG_WITH(odbc-include,
	[  --with-odbc-include=dir use odbc headers in dir],
	[if test "$withval" = "yes" -o "$withval" = "no"; then
		AC_MSG_ERROR([No argument for --with-odbc-include])
	fi])

	if test "X$with_odbc" != "X"; then
	  if test "$with_odbc" != "yes"; then
	    ac_cv_odbc_where_lib=$with_odbc
	    ac_cv_odbc_where_inc=$with_odbc/include
	  fi
	fi

	if test "X$with_odbc_include" != "X"; then
	  ac_cv_odbc_where_inc=$with_odbc_include
	fi
	if test "X$ac_cv_odbc_where_inc" = "X"; then
	  ODBC_INC_WHERE(/usr/include /usr/local/include)
	fi

	if test "X$with_odbc_lib" != "X"; then
	  ac_cv_odbc_where_lib=$with_odbc_lib
	fi
	if test "X$ac_cv_odbc_where_lib" = "X"; then
	  AC_CHECK_LIB([odbc],[SQLDisconnect],[ac_cv_odbc_where_lib="yes"],[ 
	  ODBC_LIB_WHERE(/usr/$LIB_SUBDIR /usr/local/$LIB_SUBDIR)
 	  ])
	fi

	AC_MSG_CHECKING(whether to include odbc)
	if test "X$ac_cv_odbc_where_lib" = "X" -o "X$ac_cv_odbc_where_inc" = "X"; then
	  ac_cv_found_odbc=no
	  AC_MSG_RESULT(no)
	else
	  ac_cv_found_odbc=yes
	  AC_MSG_RESULT(yes)
	  ODBC_INC_DIR=$ac_cv_odbc_where_inc
	  ODBC_LIB_DIR=$ac_cv_odbc_where_lib
	  ODBC_INC_FLAGS="-I$ac_cv_odbc_where_inc"
	  ODBC_LIB_FLAGS="-Wl,-lodbc"
	  case "$host" in
     	       *darwin*)
			ODBC_LIB_FLAGS="$ODBC_LIB_FLAGS -framework CoreFoundation"
     		;;
	  esac
	  if test "$ac_cv_odbc_where_lib" != "yes"; then
	     ODBC_LIB_FLAGS="-L$ac_cv_odbc_where_lib $ODBC_LIB_FLAGS"
	  fi
	  AC_SUBST(ODBC_INC_DIR)
	  AC_SUBST(ODBC_LIB_DIR)
	  AC_SUBST(ODBC_INC_FLAGS)
	  AC_SUBST(ODBC_LIB_FLAGS)
	  AC_DEFINE([HAVE_ODBC],[1],[libodbc])
	fi
	])


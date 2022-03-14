dnl libpcap.m4--PCAP libraries and includes
dnl Derrick Brashear
dnl from KTH krb and Arla
dnl $Id: libpcap.m4,v 1.4 2006/01/20 20:21:09 snsimon Exp $
dnl 2010/10/31 (stkn):
dnl 	rename: PCAP_INC_FLAGS -> PCAP_CPPFLAGS
dnl 	rename: PCAP_LIB_FLAGS -> PCAP_LDFLAGS	(-L flags only)
dnl 	add:    PCAP_LIBS (libs only)

AC_DEFUN([PCAP_INC_WHERE1], [
ac_cv_found_pcap_inc=no
if test -f "$1/pcap.h" ; then
  ac_cv_found_pcap_inc=yes
fi
])

AC_DEFUN([PCAP_INC_WHERE], [
   for i in $1; do
      AC_MSG_CHECKING(for pcap header in $i)
      PCAP_INC_WHERE1($i)
      if test "$ac_cv_found_pcap_inc" = "yes"; then
        ac_cv_pcap_where_inc=$i
        AC_MSG_RESULT(found)
        break
      else
        AC_MSG_RESULT(no found)
      fi
    done
])

AC_DEFUN([PCAP_LIB_WHERE1], [
saved_LIBS=$LIBS
LIBS="$saved_LIBS -L$1 -lpcap"
AC_TRY_LINK(,
[pcap_lookupdev("");],
[ac_cv_found_pcap_lib=yes],
ac_cv_found_pcap_lib=no)
LIBS=$saved_LIBS
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


AC_DEFUN([PCAP_LIB_WHERE], [
   for i in $1; do
      AC_MSG_CHECKING(for pcap library in $i)
      PCAP_LIB_WHERE1($i)
      TEST_LIBPATH($i, pcap)
      if test "$ac_cv_found_pcap_lib" = "yes" ; then
        ac_cv_pcap_where_lib=$i
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
     test -r /usr/lib/libpcap.so && ac_cv_cmu_lib_subdir=lib
   fi
   if test $ac_cv_sizeof_long -eq 8 ; then
     test -d /usr/lib64 && ac_cv_cmu_lib_subdir=lib64
   fi
 else
   ac_cv_cmu_lib_subdir=$with_lib_subdir
 fi])
 AC_SUBST(LIB_SUBDIR, $ac_cv_cmu_lib_subdir)
 ])
 

AC_DEFUN([AX_LIB_PCAP], [
AC_REQUIRE([FIND_LIB_SUBDIR])
AC_ARG_WITH(pcap,
	[  --with-pcap=PREFIX      Compile with PCAP support],
	[if test "X$with_pcap" = "X"; then
		with_pcap=yes
	fi])
AC_ARG_WITH(pcap-lib,
	[  --with-pcap-lib=dir     use pcap libraries in dir],
	[if test "$withval" = "yes" -o "$withval" = "no"; then
		AC_MSG_ERROR([No argument for --with-pcap-lib])
	fi])
AC_ARG_WITH(pcap-include,
	[  --with-pcap-include=dir use pcap headers in dir],
	[if test "$withval" = "yes" -o "$withval" = "no"; then
		AC_MSG_ERROR([No argument for --with-pcap-include])
	fi])

	if test "X$with_pcap" != "X"; then
	  if test "$with_pcap" != "yes"; then
	    ac_cv_pcap_where_lib=$with_pcap
	    ac_cv_pcap_where_inc=$with_pcap/include
	  fi
	fi

	if test "X$with_pcap_lib" != "X"; then
	  ac_cv_pcap_where_lib=$with_pcap_lib
	fi
	if test "X$ac_cv_pcap_where_lib" = "X"; then
	  PCAP_LIB_WHERE(/usr/$LIB_SUBDIR /usr/local/$LIB_SUBDIR)
	fi

	if test "X$with_pcap_include" != "X"; then
	  ac_cv_pcap_where_inc=$with_pcap_include
	fi
	if test "X$ac_cv_pcap_where_inc" = "X"; then
	  PCAP_INC_WHERE(/usr/ng/include /usr/include /usr/local/include)
	fi

	AC_MSG_CHECKING(whether to include pcap)
	if test "X$ac_cv_pcap_where_lib" != "X" -a "X$ac_cv_pcap_where_inc" != "X"; then
	  ac_cv_found_pcap=yes
	  AC_MSG_RESULT(yes)
	  PCAP_INC_DIR=$ac_cv_pcap_where_inc
	  PCAP_LIB_DIR=$ac_cv_pcap_where_lib
	  PCAP_CPPFLAGS="-I${PCAP_INC_DIR}"
	  PCAP_LDFLAGS="-L${PCAP_LIB_DIR}"
	  PCAP_LIBS="-lpcap"
	  AC_SUBST(PCAP_INC_DIR)
	  AC_SUBST(PCAP_LIB_DIR)
	  AC_SUBST(PCAP_CPPFLAGS)
	  AC_SUBST(PCAP_LDFLAGS)
	  AC_SUBST(PCAP_LIBS)
	  AC_DEFINE([HAVE_LIBPCAP],[1],[libpcap])
        else
	  ac_cv_found_pcap=no
	  AC_MSG_RESULT(no)
	fi
	])


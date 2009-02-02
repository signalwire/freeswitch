dnl libpcap.m4--PCAP libraries and includes
dnl Derrick Brashear
dnl from KTH krb and Arla
dnl $Id: libpcap.m4,v 1.4 2006/01/20 20:21:09 snsimon Exp $

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

AC_DEFUN([PCAP], [
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
	  PCAP_LIB_WHERE(/usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64)
	fi

	if test "X$with_pcap_include" != "X"; then
	  ac_cv_pcap_where_inc=$with_pcap_include
	fi
	if test "X$ac_cv_pcap_where_inc" = "X"; then
	  PCAP_INC_WHERE(/usr/ng/include /usr/include /usr/local/include)
	fi

	AC_MSG_CHECKING(whether to include pcap)
	if test "X$ac_cv_pcap_where_lib" = "X" -a "X$ac_cv_pcap_where_inc" = "X"; then
	  ac_cv_found_pcap=no
	  AC_MSG_RESULT(no)
	else
	  ac_cv_found_pcap=yes
	  AC_MSG_RESULT(yes)
	  PCAP_INC_DIR=$ac_cv_pcap_where_inc
	  PCAP_LIB_DIR=$ac_cv_pcap_where_lib
	  PCAP_INC_FLAGS="-I${PCAP_INC_DIR}"
	  PCAP_LIB_FLAGS="-L${PCAP_LIB_DIR} -lpcap"
	  AC_SUBST(PCAP_INC_DIR)
	  AC_SUBST(PCAP_LIB_DIR)
	  AC_SUBST(PCAP_INC_FLAGS)
	  AC_SUBST(PCAP_LIB_FLAGS)
	  AC_DEFINE([HAVE_LIBPCAP],[1],[libpcap])
	fi
	])


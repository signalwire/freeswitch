dnl ODBC.m4 generated automatically by aclocal 1.4a

dnl Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY, to the extent permitted by law; without
dnl even the implied warranty of MERCHANTABILITY or FITNESS FOR A
dnl PARTICULAR PURPOSE.

# Copyright (c) 1999-2000 Ajuba Solutions
# Copyright (c) 2004 ActiveState


#------------------------------------------------------------------------
# AX_PATH_ODBC --
#
#	Locate the ODBC files (includes and libraries).
#
# Arguments:
#
#	Requires:
#
# Results:
#
#	Adds ODBC include and libs to PKG info
#------------------------------------------------------------------------

dnl the alternative search directory is invoked by --with-odbcinclude
dnl and --with-odbclibrary
AC_DEFUN(AX_PATH_ODBC, [
case "$host" in
    *darwin*)
        DYNAMIC_LIB_EXTEN="dylib"
    ;;
    *cygwin* | *mingw*)
        DYNAMIC_LIB_EXTEN="dll"
    ;;
    *)
        DYNAMIC_LIB_EXTEN="so"
    ;;
esac
	AX_PATH_ODBCH
	AX_PATH_ODBCLIB
	AC_SUBST(ODBC_INCLUDE_DIR)
	AC_SUBST(ODBC_LIB_DIR)
	AC_SUBST(ODBC_LIB)
])

dnl Find the ODBC headers. This code may not work for certain
dnl intallations.
AC_DEFUN(AX_PATH_ODBCH, [
    dirlist=".. ../../ ../../../ ../../../../ ../../../../../ ../../../../../../ ../../../../../../.. ../../../../../../../.. ../../../../../../../../.. ../../../../../../../../../.."
    no_odbc=true
    ac_cv_c_odbch=""
    ODBCTYPE=""

    AC_ARG_WITH(odbcinclude, [  --with-odbcinclude      directory where odbc headers are], with_odbcinclude=${withval})

    if test x"${with_odbcinclude}" != x ; then
	# first check to see if --with-odbcinclude was specified
	list="/odbc/sql.h /ODBC/sql.h /unixodbc/sql.h /unixODBC/sql.h \
		/iodbc/isql.h"
	for i in $list ; do
	    if test -f "${with_odbcinclude}$i" ; then
		ac_cv_c_odbch=`dirname ${with_odbcinclude}$i`
		ac_cv_c_odbch=`(cd ${ac_cv_c_odbch}; pwd)`
		break
	    fi
	done
	if test x"${ac_cv_c_odbch}" = x; then
	    AC_MSG_ERROR([${with_odbcinclude} directory does not contain headers])
	fi
    fi

    dnl ===============================
    dnl IODBC... I have not tested this a whole lot
    dnl ===============================
    dn`l Check in the source tree
    for i in $dirlist; do
	if test -f "${srcdir}/$i/postodbc/isql.h" ; then
	    ac_cv_c_odbch=`(cd ${srcdir}/$i/postodbc/; pwd)`
	    ODBCTYPE=iodbc
	fi
    done

    dnl Check a few common places in the file system
    if test x"${ac_cv_c_odbch}" = x ; then
	for i in \
		/usr/local/mysql/include \
		/usr/local/include/iodbc \
		/usr/local/share/iodbc \
		${prefix}/include/iodbc \
		/usr/local/include \
		/usr/include \
		${prefix}/include ; do
	    if test -f $i/isql.h; then
		ac_cv_c_odbch=`(cd $i; pwd)`
		ODBCTYPE=iodbc
	    fi
	done
    fi

    dnl check if its installed with the compiler
    if test x"${ac_cv_c_odbch}" = x ; then
	dnl Get the path to the compiler, strip off any args in ${CC}
	ccprog=`echo ${CC} | sed -e 's: .*::'`
	ccpath=`which ${ccprog} | sed -e 's:/bin/.*::'`/include/iodbc
	if test -f $ccpath/isql.h; then
	    ac_cv_c_odbch=$ccpath
	    ODBCTYPE=iodbc
	fi
    fi

    dnl see if one is installed
    if test x"${ac_cv_c_odbch}" = x ; then
	AC_CHECK_HEADER(isql.h, ac_cv_c_odbch=installed, ac_cv_c_odbch="")
	if test x"${ac_cv_c_odbch}" != x ; then
	    ODBCTYPE=iodbc
	fi
    fi

    dnl ===============================
    dnl unixODBC or Intersolv... I have tested this
    dnl ===============================
    dnl Check for unixODBC headers
    if test x"${ac_cv_c_odbch}" = x ; then
	for i in \
		/opt/odbc/include \
		/opt/ODBC/include \
		${prefix}/include/odbc \
		${prefix}/include/ODBC \
		/opt/unixodbc/include \
		/opt/unixODBC/include \
		${prefix}/include/unixodbc \
		${prefix}/include/unixODBC \
		/usr/local/mysql/include \
		/usr/local/include \
		/usr/include \
		${prefix}/include ; do
	    if test -f "$i/sql.h"; then
		ac_cv_c_odbch=`(cd $i; pwd)`
		ODBCTYPE=unixODBC
	    fi
	done
    fi

    dnl see if one is installed
    if test x"${ac_cv_c_odbch}" = x ; then
	AC_CHECK_HEADER(sql.h, ac_cv_c_odbch=installed, ac_cv_c_odbch="")
	if test x"${ac_cv_c_odbch}" != x ; then
	    ODBCTYPE=unixODBC
	fi
    fi

    dnl Figure out whether this is unixODBC or Intersolv
    if test x"${ODBCTYPE}" = xunixODBC ; then
	if test -f "${ac_cv_c_odbch}/odbcver.h" ; then
	    ODBCTYPE=intersolv
	fi
    fi

    ODBC_INCLUDE_DIR=""
    if test x"${ac_cv_c_odbch}" = x ; then
	AC_MSG_ERROR([Cannot find any ODBC headers])
    else
	if test x"${ODBCTYPE}" = x ; then
	    if test -f "${ac_cv_c_odbch}/odbcver.h" ; then
		ODBCTYPE=intersolv
	    elif test -f "${ac_cv_c_odbch}/sql.h" ; then
		ODBCTYPE=unixODBC
	    elif test -f "${ac_cv_c_odbch}/isql.h" ; then
		ODBCTYPE=iodbc
	    fi
	fi
    fi

    AC_MSG_CHECKING([for ODBC headers files])
    if test x"${ac_cv_c_odbch}" != x ; then
	no_odbc=""
	if test x"${ac_cv_c_odbch}" != xinstalled ; then
	    AC_MSG_RESULT(${ac_cv_c_odbch})
	    ODBC_INCLUDE_DIR="-I${ac_cv_c_odbch}"
	else
	    AC_MSG_RESULT(none)
	fi
    fi

    AC_SUBST(ODBC_INCLUDE_DIR)
    AC_SUBST(ODBCTYPE)
])

dnl Find the ODBC library. This code may not work for certain
dnl intallations.
AC_DEFUN(AX_PATH_ODBCLIB, [
    dirlist=".. ../../ ../../../ ../../../../ ../../../../../ ../../../../../../ ../../../../../../.. ../../../../../../../.. ../../../../../../../../.. ../../../../../../../../../.."
    no_odbc=true
    ac_cv_c_odbc_libdir=""
    ac_cv_c_odbc_lib=""
    ODBC_LIB_NAME=""

    AC_ARG_WITH(odbclibrary, [  --with-odbclibrary 	  directory where odbc libraries are], with_odbclibrary=${withval})

    dnl first check to see if --with-odbclibrary was specified
    if test x"${with_odbclibrary}" != x ; then
	if test -f ${with_odbclibrary} ; then
	    ac_cv_c_odbc_libdir=`echo ${with_odbclibrary} | sed -e 's:/libpsqlodbc.*::'`
	    ac_cv_c_odbc_libdir=`(cd ${ac_cv_c_odbc_libdir}; pwd)`
	    ac_cv_c_odbc_lib=`echo ${with_odbclibrary} | sed -e 's:.*/::'`
	elif test -f ${with_odbclibrary}/libodbc.${DYNAMIC_LIB_EXTEN} ; then
	    ac_cv_c_odbc_libdir=`(cd ${with_odbclibrary}; pwd)`
	    ac_cv_c_odbc_lib='-lodbc'
	elif test -f ${with_odbclibrary}/libpsqlodbc.${DYNAMIC_LIB_EXTEN} ; then
	    ac_cv_c_odbc_libdir=`(cd ${with_odbclibrary}; pwd)`
	elif test -f ${with_odbclibrary}/iodbc/libpsqlodbc.${DYNAMIC_LIB_EXTEN} ; then
	    ac_cv_c_odbc_libdir=`(cd ${with_odbclibrary}/iodbc; pwd)`
	    ac_cv_c_odbc_lib='-llibpsqlodbc'
	else
	    AC_MSG_ERROR([${with_odbclibrary} directory does not contain library])
	fi
    fi

    dnl Check for multiple library names depending on the ODBC type
    if test x"${ODBCTYPE}" = xiodbc ; then
	liblist="libpsqlodbc libmysqlodbc librbodbc libiodbc iodbc"
	libpathlist="/usr/local/lib/iodbc ${prefix}/iodbc/lib \
	    ${prefix}/lib/iodbc /usr/local/mysql/lib /usr/local/lib /usr/lib ${prefix}/lib"
    elif test x"${ODBCTYPE}" = xunixODBC ; then 
	liblist="libodbc"
	libpathlist="/opt/unixodbc/lib /opt/unixODBC/lib /usr/include/odbc \
	    /usr/include/ODBC /usr/local/mysql/lib /usr/local/lib \
	    ${prefix}/lib"
    elif test x"${ODBCTYPE}" = xintersolv ; then 
	liblist="libodbc"
	libpathlist="/opt/odbc/lib /opt/ODBC/lib /usr/include/unixodbc \
	    /usr/include/unixODBC /usr/local/mysql/lib /usr/local/lib \
	    ${prefix}/lib"
    else
	AC_MSG_ERROR([ODBC support could not be identified.])
    fi

    for libname in $liblist; do
	dnl Check in the build tree
	for i in $dirlist; do
	    if test -f "$i/postodbc/${libname}.${DYNAMIC_LIB_EXTEN}" ; then
		ac_cv_c_odbc_libdir=`(cd $i/postodbc/; pwd)`
		break
	    fi
	done

	dnl Check a few common places in the file system
	if test x"${ac_cv_c_odbc_libdir}" = x ; then
	    for i in $libpathlist; do
		if test -f $i/${libname}.${DYNAMIC_LIB_EXTEN}; then
		    ac_cv_c_odbc_libdir=`(cd $i; pwd)`
		    break
		fi
	    done
	fi

	dnl check if its installed with the compiler
	if test x"${ac_cv_c_odbc_libdir}" = x ; then
	    dnl Get the path to the compiler, strip off any args in ${CC}
	    ccprog=`echo ${CC} | sed -e 's: .*::'`
	    ccpath=`which ${ccprog} | sed -e 's:/bin/.*::'`/lib
	    if test -f $ccpath/${libname}.${DYNAMIC_LIB_EXTEN}; then
		ac_cv_c_odbc_libdir=$ccpath
	    fi
	fi

	dnl see if one is installed
	if test x"${ac_cv_c_odbc_libdir}" = x ; then
	    AC_CHECK_LIB(${libname}, main)
	fi

	dnl do not keep searching for the other libraries, cause we found one
	if test x"${ac_cv_c_odbc_libdir}" != x ; then
	    ac_cv_c_odbc_lib=${libname}
	    break
	fi
    done

    ODBC_LIB_DIR=""
    AC_MSG_CHECKING(for ODBC library files)
    if test x"${ac_cv_c_odbc_libdir}" = x ; then
	AC_MSG_ERROR([Cannot find an ODBC library path])
    fi
    if test x"${ac_cv_c_odbc_lib}" = x ; then
	AC_MSG_ERROR([Cannot find an ODBC library])
    fi
    if test x"${ac_cv_c_odbc_libdir}" != x ; then
	AC_MSG_RESULT(${ac_cv_c_odbc_libdir})
	ODBC_LIB_DIR="-L${ac_cv_c_odbc_libdir}"
    fi

    AC_MSG_CHECKING(for ODBC library name)
    ODBC_LIB=-l`echo $ac_cv_c_odbc_lib | sed -e 's:^lib::'`
    AC_MSG_RESULT($ODBC_LIB)

    if test x"${ODBCTYPE}" = xiodbc ; then
	AC_MSG_RESULT(Using IODBC driver)
	AC_DEFINE(HAVE_IODBC)
	ODBC_LIB="$ODBC_LIB -liodbcinst"
    fi

    if test x"${ODBCTYPE}" = xunixODBC ; then
	AC_MSG_RESULT(Using UnixODBC driver)
	AC_DEFINE(HAVE_UNIXODBC)
	AC_DEFINE(DONT_TD_VOID)
    fi

    if test x"${ODBCTYPE}" = xintersolv ; then
	AC_MSG_RESULT(Using Intersolv driver)
	AC_DEFINE(HAVE_INTERSOLV)
    fi

    AC_SUBST(ODBC_LIB)
    AC_SUBST(ODBC_LIB_DIR)
])



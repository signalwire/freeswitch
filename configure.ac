#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# Must change all of the below together
# For a release, set revision for that tagged release as well and uncomment
AC_INIT([freeswitch], [1.4.18], bugs@freeswitch.org)
AC_SUBST(SWITCH_VERSION_MAJOR, [1])
AC_SUBST(SWITCH_VERSION_MINOR, [4])
AC_SUBST(SWITCH_VERSION_MICRO, [18])
AC_SUBST(SWITCH_VERSION_REVISION, [])
AC_SUBST(SWITCH_VERSION_REVISION_HUMAN, [])

AC_CONFIG_FILES([src/include/switch_version.h.in:src/include/switch_version.h.template])

AC_CONFIG_AUX_DIR(build/config)
AM_INIT_AUTOMAKE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_SRCDIR([src/switch.c])
AC_CONFIG_HEADER([src/include/switch_private.h])
AC_CONFIG_HEADER([libs/esl/src/include/esl_config_auto.h])
AC_CONFIG_HEADER([libs/xmlrpc-c/xmlrpc_amconfig.h])

AC_PREFIX_DEFAULT(/usr/local/freeswitch)
# AC_PREFIX_DEFAULT does not get expanded until too late so we need to do this to use prefix in this script
if test "x$prefix" = "xNONE" ; then
    prefix='/usr/local/freeswitch'
fi

AC_CANONICAL_HOST

# Absolute source/build directory
switch_srcdir=`(cd $srcdir && pwd)`
switch_builddir=`pwd`
AC_SUBST(switch_srcdir)
AC_SUBST(switch_builddir)

#
# --enable-64 has been moved up higher prior to AC_PROG_CC so that we can tuck in the -m64 flag 
# so devs on with Solaris wanting to build 64bit can not bother with adding any additional 
# flags on the ./configure line.  User friendly.
#

#  Enable 64 bit build
AC_ARG_ENABLE(64,
[AC_HELP_STRING([--enable-64],[build with 64 bit support])],[enable_64="$enable_64"],[enable_64="no"])

if test "${enable_64}" = "yes"; then
   case "$host" in
        *-solaris2*)
	    # All three have to have -m64 for AC_PROG_CC to pick the right libtool
            CFLAGS="$CFLAGS -m64"
            LDFLAGS="$LDFLAGS -m64"
            CXXFLAGS="$CXXFLAGS -m64"
        ;;
        *)
        ;;
   esac
fi

# Whether to follow FHS
AC_ARG_ENABLE([fhs],
  [AS_HELP_STRING([--enable-fhs],
    [Follow the FHS when placing files and directories])],,
  [enable_fhs="no"])
AM_CONDITIONAL([ENABLE_FHS],[test "${enable_fhs}" = "yes"])

if test "$localstatedir" = "\${prefix}/var" ; then
    rundir="$prefix/run"
    logdir="${prefix}/log"
else
    rundir="$localstatedir/run/freeswitch"
    logdir="$localstatedir/log/freeswitch"
fi

default_certsdir="$prefix/certs"
default_dbdir="$prefix/db"
default_grammardir="$prefix/grammar"
default_htdocsdir="$prefix/htdocs"
default_logfiledir="$logdir"
default_modulesdir="$prefix/mod"
default_pkgconfigdir="$libdir/pkgconfig"
default_recordingsdir="$prefix/recordings"
default_runtimedir="$rundir"
default_scriptdir="$prefix/scripts"
default_soundsdir="$prefix/sounds"
default_storagedir="$prefix/storage"
default_cachedir="$prefix/cache"

if test "$enable_fhs" = yes; then
  prefix="/usr" exec_prefix="$prefix"
  libdir="$(eval echo "$libdir")"
  sysconfdir="/etc/freeswitch"
  localstatedir="/var"
  default_certsdir="/etc/freeswitch/tls"
  default_dbdir="/var/lib/freeswitch/db"
  default_grammardir="/usr/share/freeswitch/grammar"
  default_htdocsdir="/usr/share/freeswitch/htdocs"
  default_logfiledir="/var/log/freeswitch"
  default_modulesdir="$libdir/freeswitch/mod"
  default_pkgconfigdir="$libdir/pkgconfig"
  default_recordingsdir="/var/lib/freeswitch/recordings"
  default_runtimedir="/var/run/freeswitch"
  default_scriptdir="/usr/share/freeswitch/scripts"
  default_soundsdir="/usr/share/freeswitch/sounds"
  default_storagedir="/var/lib/freeswitch/storage"
  default_cachedir="/var/cache/freeswitch"
fi

# Where to install the modules
AC_ARG_WITH([modinstdir],
	[AS_HELP_STRING([--with-modinstdir=DIR], [Install modules into this location (default: $prefix/mod)])], [modulesdir="$withval"], [modulesdir="${default_modulesdir}"])
AC_SUBST(modulesdir)
AC_DEFINE_UNQUOTED([SWITCH_MOD_DIR],"${modulesdir}",[where to install the modules to])

# Where to put pidfile
AC_ARG_WITH([rundir],
	[AS_HELP_STRING([--with-rundir=DIR], [Put pidfile into this location (default: $prefix/run)])], [runtimedir="$withval"], [runtimedir="${default_runtimedir}"])
AC_SUBST(runtimedir)
AC_DEFINE_UNQUOTED([SWITCH_RUN_DIR],"${runtimedir}",[where to put pidfile to])

AC_ARG_WITH([logfiledir],
	[AS_HELP_STRING([--with-logfiledir=DIR], [Put logfiles into this location (default: $localstatedir/log)])], [logfiledir="$withval"], [logfiledir="${default_logfiledir}"])
AC_SUBST(logfiledir)
AC_DEFINE_UNQUOTED([SWITCH_LOG_DIR],"${logfiledir}",[where to put log files])

AC_ARG_WITH([dbdir],
	[AS_HELP_STRING([--with-dbdir=DIR], [Put database files into this location (default: $prefix/db)])], [dbdir="$withval"], [dbdir="${default_dbdir}"])
AC_SUBST(dbdir)
AC_DEFINE_UNQUOTED([SWITCH_DB_DIR],"${dbdir}",[where to put db files])

AC_ARG_WITH([htdocsdir],
	[AS_HELP_STRING([--with-htdocsdir=DIR], [Put html files into this location (default: $prefix/htdocs)])], [htdocsdir="$withval"], [htdocsdir="${default_htdocsdir}"])
AC_SUBST(htdocsdir)
AC_DEFINE_UNQUOTED([SWITCH_HTDOCS_DIR],"${htdocsdir}",[where to put htdocs files])

AC_ARG_WITH([soundsdir],
	[AS_HELP_STRING([--with-soundsdir=DIR], [Put sound files into this location (default: $prefix/sounds)])], [soundsdir="$withval"], [soundsdir="${default_soundsdir}"])
AC_SUBST(soundsdir)
AC_DEFINE_UNQUOTED([SWITCH_SOUNDS_DIR],"${soundsdir}",[where to put sounds files])

AC_ARG_WITH([grammardir],
	[AS_HELP_STRING([--with-grammardir=DIR], [Put grammar files into this location (default: $prefix/grammar)])], [grammardir="$withval"], [grammardir="${default_grammardir}"])
AC_SUBST(grammardir)
AC_DEFINE_UNQUOTED([SWITCH_GRAMMAR_DIR],"${grammardir}",[where to put grammar files])

AC_ARG_WITH([certsdir],
	[AS_HELP_STRING([--with-certsdir=DIR], [Put certs files into this location (default: $prefix/certs)])], [certsdir="$withval"], [certsdir="${default_certsdir}"])
AC_SUBST(certsdir)
AC_DEFINE_UNQUOTED([SWITCH_CERTS_DIR],"${certsdir}",[where to put certs files])

AC_ARG_WITH([scriptdir],
	[AS_HELP_STRING([--with-scriptdir=DIR], [Put script files into this location (default: $prefix/scripts)])], [scriptdir="$withval"], [scriptdir="${default_scriptdir}"])
AC_SUBST(scriptdir)
AC_DEFINE_UNQUOTED([SWITCH_SCRIPT_DIR],"${scriptdir}",[where to put script files])

AC_ARG_WITH([recordingsdir],
	[AS_HELP_STRING([--with-recordingsdir=DIR], [Put recordings files into this location (default: $prefix/recordings)])], [recordingsdir="$withval"], [recordingsdir="${default_recordingsdir}"])
AC_SUBST(recordingsdir)
AC_DEFINE_UNQUOTED([SWITCH_RECORDINGS_DIR],"${recordingsdir}",[where to put recording files])

AC_ARG_WITH([storagedir],
	[AS_HELP_STRING([--with-storagedir=DIR], [Put storage files into this location (default: $prefix/storage)])], [storagedir="$withval"], [storagedir="${default_storagedir}"])
AC_SUBST(storagedir)
AC_DEFINE_UNQUOTED([SWITCH_STORAGE_DIR],"${storagedir}",[where to put storage files])

AC_ARG_WITH([cachedir],
	[AS_HELP_STRING([--with-cachedir=DIR], [Put cache files into this location (default: $prefix/cache)])], [cachedir="$withval"], [cachedir="${default_cachedir}"])
AC_SUBST(cachedir)
AC_DEFINE_UNQUOTED([SWITCH_CACHE_DIR],"${cachedir}",[where to put cache files])

if test "$sysconfdir" = "\${prefix}/etc" ; then
    confdir="$prefix/conf"
else
    confdir="$sysconfdir"
fi

sysconfdir="$confdir"

AC_SUBST(confdir)
AC_DEFINE_UNQUOTED([SWITCH_CONF_DIR],"${confdir}",[directory for configuration files])

AC_SUBST(libdir)
AC_SUBST(bindir)
AC_SUBST(includedir)

AC_ARG_WITH([pkgconfigdir],
	[AS_HELP_STRING([--with-pkgconfigdir=DIR], [Installation directory for pkgconfig file (default: ${libdir}/pkgconfig)])],
	[case "${withval}" in
	 yes|no) AC_MSG_ERROR([Invalid value ${withval} for option --with-pkgconfigdir]) ;;
	 *) pkgconfigdir="${withval}" ;;
	 esac
	],
	[pkgconfigdir="${default_pkgconfigdir}"]
)
AC_SUBST([pkgconfigdir])

#Set default language
AC_LANG_C
# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

#check if the g++ compiler works
AC_CACHE_CHECK([whether the C++ compiler works], [ac_cv_sys_cxx_works], [
    AC_LANG_PUSH([C++])
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() { }])], [ac_cv_sys_cxx_works=yes],
        [ac_cv_sys_cxx_works=no])
    AC_LANG_POP([C++])
    ])
[ if [ "x$ac_cv_sys_cxx_works" = "xno" ]; then ]
    AC_MSG_FAILURE([The C++ compiler does not work. Please (re)install the C++ compiler])
[ fi ]

AC_PROG_AWK
AC_PROG_MAKE_SET
AC_PROG_INSTALL

#override some default libtool behavior and invoke AC_PROG_LIBTOOL (see http://lists.gnu.org/archive/html/libtool/2007-03/msg00000.html)
m4_defun([_LT_AC_LANG_F77_CONFIG], [:])
m4_defun([_LT_AC_LANG_GCJ_CONFIG], [:])
m4_defun([_LT_AC_LANG_RC_CONFIG], [:])
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

#Check for compiler vendor
AX_COMPILER_VENDOR

# Set CC_FOR_BUILD
if test "x${cross_compiling}" = "xyes"; then
    CC_FOR_BUILD=${CC_FOR_BUILD-gcc}
    case "$host" in
    	 arm*-linux-gnueabi*|arm*-*-linux-gnueabi*)
		# spandsp modem
		ac_cv_file__dev_ptmx=yes
		# libjs
		export ac_cv_va_copy=yes
		# srtp
		export ac_cv_file__dev_urandom=yes
		# rpl_malloc
		export ac_cv_func_realloc_0_nonnull=yes
		export ac_cv_func_malloc_0_nonnull=yes
		# apr
		export ac_cv_func_setpgrp_void=yes
		export ac_cv_file__dev_zero=yes
		export apr_cv_tcp_nodelay_with_cork=yes
		export ac_cv_file_dbd_apr_dbd_mysql_c=no
		export ac_cv_sizeof_ssize_t=4
		export apr_cv_mutex_recursive=yes
		export ac_cv_func_pthread_rwlock_init=yes
		export apr_cv_type_rwlock_t=yes
		export apr_cv_process_shared_works=yes
		export apr_cv_mutex_robust_shared=yes
	 ;;
    esac
else
    CC_FOR_BUILD='$(CC)'
fi
AC_SUBST(CC_FOR_BUILD)

if test -n "$lt_sysroot" ; then
   APR_ADDTO(CFLAGS, --sysroot=$lt_sysroot)
   APR_ADDTO(CXXFLAGS, --sysroot=$lt_sysroot)
   APR_ADDTO(CPPFLAGS, --sysroot=$lt_sysroot)
   APR_ADDTO(LDFLAGS, --sysroot=$lt_sysroot)
   PKG_CONFIG_SYSROOT_DIR=$lt_sysroot
fi

#  Optimize
AC_ARG_ENABLE(optimization,
[AC_HELP_STRING([--enable-optimization],[Set if you want us to add max optimising compiler flags])],[enable_optimizer="$enableval"],[enable_optimizer="no"])

if test "${enable_optimizer}" = "yes" ; then
        AC_DEFINE([OPTIMZER],[],[Enable Optimization.])
        AX_CC_MAXOPT
fi

# set defaults for use on all platforms
SWITCH_AM_CFLAGS="-I${switch_srcdir}/src/include -I${switch_builddir}/src/include -I${switch_srcdir}/libs/libteletone/src"
SWITCH_AM_CXXFLAGS="-I${switch_srcdir}/src/include -I${switch_builddir}/src/include -I${switch_srcdir}/libs/libteletone/src"
SWITCH_AM_LDFLAGS="-lm"

#set SOLINK variable based on compiler and host
if test "x${ax_cv_c_compiler_vendor}"  =  "xsun" ; then
    SOLINK="-Bdynamic -dy -G"
elif test "x${ax_cv_c_compiler_vendor}" = "xclang" ; then
    case "$host" in
        *darwin*)
            SOLINK="-dynamic -force-flat-namespace"
        ;;
        *)
            SOLINK="-shared -Xlinker -x"
        ;;

    esac
elif test "x${ax_cv_c_compiler_vendor}" = "xgnu" ; then
    case "$host" in
# older Xcode test for darwin, Xcode 4/5 use clang above
        *darwin*)
            SOLINK="-dynamic -bundle -force-flat-namespace"
        ;;
        *-solaris2*)
            SOLINK="-shared -Xlinker"
        ;;
        *)
            SOLINK="-shared -Xlinker -x"
        ;;
    esac
elif test "x${ax_cv_c_compiler_vendor}" = "xintel" ; then
    case "$host" in
        *)
            SOLINK="-shared -Xlinker -x"
        ;;
    esac
else
    AC_ERROR([Please update configure.in with SOLINK values for your compiler])
fi

##
# detect libtool major version,
# set libtool library extension based on this
# to work around some brokeness when using 'la' with libtool-1.5
#
AC_MSG_CHECKING([libtool major version])
libtool="${switch_builddir}/libtool"
LIBTOOL_MAJOR_VERSION="`$libtool --version 2>/dev/null| sed -e 's/([[^)]]*)//g;s/^[[^0-9]]*//;s/[[- ]].*//g;q'| awk  'BEGIN { FS = "." } { print $1 }' `"
if test -z "$LIBTOOL_MAJOR_VERSION" ; then
	LIBTOOL_MAJOR_VERSION="`sed -n -e '/^VERSION/{s/^.*=\"\{0,1\}\([[0-9]]\{1,\}\)\..*/\1/;p;}' ${switch_srcdir}/build/config/ltmain.sh`"
fi
if test -z "$LIBTOOL_MAJOR_VERSION" ; then
	AC_MSG_ERROR([Failed to detect your libtool version, please open a bug report on http://jira.freeswitch.org/])
fi
AC_MSG_RESULT([${LIBTOOL_MAJOR_VERSION}])

LIBTOOL_LIB_EXTEN=so

if test "${LIBTOOL_MAJOR_VERSION}" = "2" ; then
   LIBTOOL_LIB_EXTEN="la"
fi
AC_MSG_RESULT([using libtool library extension... ${LIBTOOL_LIB_EXTEN}])

if test "$ax_cv_c_compiler_vendor" = "gnu"; then
  saved_CFLAGS="$CFLAGS"
  AC_CACHE_CHECK([whether compiler supports -Wno-unused-result],
    [ac_cv_gcc_supports_w_no_unused_result], [
      CFLAGS="$CFLAGS -Wno-unused-result -Wno-error=unused-result"
      AC_TRY_COMPILE([],[return 0;],
        [ac_cv_gcc_supports_w_no_unused_result=yes],
        [ac_cv_gcc_supports_w_no_unused_result=no])])
  CFLAGS="$saved_CFLAGS"
  AC_MSG_RESULT($ac_cv_gcc_supports_w_no_unused_result)
fi

# tweak compiler specific flags
if test "x${ax_cv_c_compiler_vendor}"  =  "xsun" ; then
    APR_ADDTO(SWITCH_AM_CFLAGS, -KPIC)
    APR_ADDTO(SWITCH_AM_CFLAGS, -DPIC)
    APR_ADDTO(SWITCH_AM_CFLAGS, -erroff=E_END_OF_LOOP_CODE_NOT_REACHED)
    APR_ADDTO(SWITCH_AM_CFLAGS, -errtags=yes)
    APR_ADDTO(SWITCH_AM_CFLAGS, -D__FUNCTION__=__func__ )
    APR_ADDTO(SWITCH_AM_CFLAGS, -mt)
    APR_ADDTO(SWITCH_AM_CXXFLAGS, -errtags=yes)
    APR_ADDTO(SWITCH_AM_CXXFLAGS, -KPIC)
    APR_ADDTO(SWITCH_AM_CXXFLAGS, -DPIC)
    APR_ADDTO(SWITCH_AM_CXXFLAGS, "-features=extensions")
    APR_ADDTO(SWITCH_AM_CXXFLAGS, -D__FUNCTION__=__func__)
    APR_ADDTO(SWITCH_AM_CXXFLAGS, -mt)

    APR_ADDTO(SWITCH_AM_LDFLAGS, -R${prefix}/lib)
    if test "${enable_64}" = "yes"; then
        APR_ADDTO(SWITCH_AM_CFLAGS, -m64)
        APR_ADDTO(SWITCH_AM_CXXFLAGS, -m64)
        APR_ADDTO(SWITCH_AM_LDFLAGS, -m64)
	LIBS="$LIBS -m64"
    fi
elif test "x${ax_cv_c_compiler_vendor}" = "xclang" ; then
    APR_ADDTO(SWITCH_AM_CFLAGS, -fPIC)
    APR_ADDTO(SWITCH_AM_CXXFLAGS, -fPIC)
    APR_ADDTO(SWITCH_AM_CFLAGS, -Werror)
elif test "x${ax_cv_c_compiler_vendor}" = "xgnu" ; then
    APR_ADDTO(SWITCH_AM_CFLAGS, -fPIC)
    APR_ADDTO(SWITCH_AM_CXXFLAGS, -fPIC)
    if test "$ac_cv_gcc_supports_w_no_unused_result" = yes; then
      APR_ADDTO(SWITCH_AM_CFLAGS, -Werror)
    fi
    if test "${enable_64}" = "yes"; then
	case "$host" in
             *darwin*)
                osxvrm=`sw_vers -productVersion`          # Get version.release.modlevel
	        osxrel=`echo $osxvrm | cut -d. -f2`       # Get release only
                if test "$osxrel" -ge 4; then             # 10.4 and up are x64
		   APR_ADDTO(CFLAGS, -arch x86_64)
		   APR_ADDTO(LDFLAGS, -arch x86_64)
		   APR_ADDTO(CXXFLAGS, -arch x86_64)
                fi
		;;
             *-solaris2*)
		APR_ADDTO(CFLAGS, -m64)
		APR_ADDTO(LDFLAGS, -m64)
		APR_ADDTO(CXXFLAGS, -m64)
	        ;;
	     *)
	     	LIBS="$LIBS -m64"
	     ;;
	 esac
    fi
fi

#  Enable clang address sanitizer bit build
AC_ARG_ENABLE(address_sanitizer,
  [AC_HELP_STRING([--enable-address-sanitizer],[build with address sanitizer])],
  [enable_address_sanitizer="$enable_address_sanitizer"],
  [enable_address_sanitizer="no"])

if test "${enable_address_sanitizer}" = "yes"; then
  if test "x${ax_cv_c_compiler_vendor}"  =  "xclang" ; then
    APR_ADDTO(CFLAGS, -fsanitize=address -fno-omit-frame-pointer)
    APR_ADDTO(CXXFLAGS, -fsanitize=address -fno-omit-frame-pointer)
    APR_ADDTO(LDFLAGS, -fsanitize=address)
  fi
fi


#
# gcc visibility cflag checks
#
AC_ARG_ENABLE([visibility],
	[AS_HELP_STRING([--disable-visibility], [Disable or enable API visibility support (default: use if available)])],
	[enable_visibility="${enableval}"],
	[enable_visibility="detect"]
)
HAVE_VISIBILITY="no"

if test "x${enable_visibility}" != "xno" ; then

	case "${ax_cv_c_compiler_vendor}" in
	gnu)
		save_CFLAGS="${CFLAGS}"
		CFLAGS="${CFLAGS} -fvisibility=hidden"
		AC_MSG_CHECKING([whether the compiler supports -fvisibility=hidden])
		AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM(
				[int foo __attribute__ ((visibility("default")));],
				[;]
			)],

			[AC_MSG_RESULT([yes])
			 APR_ADDTO([SWITCH_AM_CFLAGS],   [-fvisibility=hidden -DSWITCH_API_VISIBILITY=1 -DHAVE_VISIBILITY=1])
			 APR_ADDTO([SWITCH_AM_CXXFLAGS], [-fvisibility=hidden -DSWITCH_API_VISIBILITY=1 -DHAVE_VISIBILITY=1])
			 HAVE_VISIBILITY="yes"],

			[AC_MSG_RESULT([no])]
		)
		CFLAGS="${save_CFLAGS}"
		;;

	sun)
	#	save_CFLAGS="${CFLAGS}"
	#	CFLAGS="${CFLAGS} -xldscope=hidden"
	#	AC_MSG_CHECKING([whether the compiler supports -xldscope=hidden])
	#	AC_COMPILE_IFELSE(
	#		[AC_LANG_PROGRAM(
	#			[int foo __attribute__ ((visibility("default")));],
	#			[;]
	#		)],
	#
	#		[AC_MSG_RESULT([yes])
	#		 APR_ADDTO([SWITCH_AM_CFLAGS],   [-xldscope=hidden -DSWITCH_API_VISIBILITY=1 -DHAVE_VISIBILITY=1])
	#		 APR_ADDTO([SWITCH_AM_CXXFLAGS], [-xldscope=hidden -DSWITCH_API_VISIBILITY=1 -DHAVE_VISIBILITY=1])
	#		 HAVE_VISIBILITY="yes"],
	#
	#		[AC_MSG_RESULT([no])]
	#	)
	#	CFLAGS="${save_CFLAGS}"
		;;

	*)
		if test "x${enable_visibility}" = "xyes" ; then
			AC_MSG_ERROR([Non-GNU / SUN compilers are currently unsupported])
		else
			AC_MSG_WARN([Non-GNU / SUN compilers are currently unsupported])
		fi
		;;
	esac

	#
	# visibility explicitly requested but not supported by this compiler => error
	#
	if test "x${enable_visibility}" = "xyes" -a "x${HAVE_VISIBILITY}" = "xno" ; then
		AC_MSG_ERROR([API visibility not supported by this compiler])
	fi
fi

# Enable debugging (default: on)
# (rename option if the default is changed)
AC_ARG_ENABLE(debug,
[AC_HELP_STRING([--disable-debug],[build without debug information])],[enable_debug="$enableval"],[enable_debug="yes"])

if test "${enable_debug}" = "yes"; then
        AC_DEFINE([DEBUG],[],[Enable extra debugging.])
	saved_CFLAGS="$CFLAGS"
	CFLAGS=
        AX_CFLAGS_WARN_ALL_ANSI
	SWITCH_ANSI_CFLAGS=$CFLAGS
	CFLAGS="$saved_CFLAGS"	

	if test "x${ax_cv_c_compiler_vendor}" = "xgnu" ; then
	    if test "$ac_cv_gcc_supports_w_no_unused_result" = yes; then
	      APR_ADDTO(SWITCH_AM_CFLAGS, -Wno-unused-result)
	    fi
	    APR_ADDTO(SWITCH_AM_CFLAGS, -g -ggdb)
	    export DEBUG_CFLAGS="-g -ggdb"
	fi

fi

AC_ARG_ENABLE(cpp,
[AC_HELP_STRING([--disable-cpp],[build without cpp code])],[enable_cpp="$enableval"],[enable_cpp="yes"])

AM_CONDITIONAL([ENABLE_CPP],[test "${enable_cpp}" = "yes"])

AM_CONDITIONAL([DISABLE_CC],[test "${disable_cc}" = "yes"])

AC_ARG_ENABLE([system-xmlrpc-c],
  [AS_HELP_STRING([--enable-system-xmlrpc-c],
    [use system lib for xmlrpc-c])],,
  [enable_xmlrpcc="no"])

if test "${enable_xmlrpcc}" = "yes" ; then
SYS_XMLRPC_CFLAGS=`xmlrpc-c-config --cflags`
SYS_XMLRPC_LDFLAGS=`xmlrpc-c-config --libs`
fi
AC_SUBST(SYS_XMLRPC_CFLAGS)
AC_SUBST(SYS_XMLRPC_LDFLAGS)
AM_CONDITIONAL([SYSTEM_XMLRPCC],[test "${enable_xmlrpcc}" = "yes"])

AC_ARG_ENABLE([system-lua],[AS_HELP_STRING([--enable-system-lua],[use system lib for lua])],,[enable_system_lua="no"])
PKG_CHECK_MODULES([LUA],[lua5.2],[have_lua=yes],[have_lua=no])
if test "${have_lua}" = "no" ; then
   PKG_CHECK_MODULES([LUA],[lua5.1],[have_lua=yes],[have_lua=no])
fi
if test "${have_lua}" = "no" ; then
   PKG_CHECK_MODULES([LUA],[lua],[have_lua=yes],[have_lua=no])
fi
if test x"${LUA_LIBS}" = x"" ; then
   LUA_LIBS="-llua"
fi
AM_CONDITIONAL([SYSTEM_LUA],[test "${enable_system_lua}" = "yes"])

AC_ARG_ENABLE(srtp,
[AC_HELP_STRING([--disable-srtp],[build without srtp support])],[enable_srtp="$enableval"],[enable_srtp="yes"])

AM_CONDITIONAL([ENABLE_SRTP],[test "${enable_srtp}" = "yes"])

have_openal=no
AC_CHECK_LIB(openal, alMidiGainSOFT, [have_openal="yes"])
AM_CONDITIONAL([HAVE_OPENAL],[test "${have_openal}" = "yes"])

AC_ARG_ENABLE(zrtp,
	[AS_HELP_STRING([--enable-zrtp], [Compile with zrtp Support])],,[enable_zrtp="no"])
if test "x$enable_zrtp" = "xyes" ; then
   LIBS="-lpthread $LIBS"
   APR_ADDTO(SWITCH_AM_CFLAGS, -DENABLE_ZRTP)
fi

PA_LIBS=

PKG_CHECK_MODULES(JACK, jack, have_jack=yes, have_jack=no)
if test "x$have_jack" = "xyes" ; then
PA_LIBS+=$JACK_LIBS
fi

AC_CHECK_LIB(asound, snd_pcm_open, have_alsa=yes, have_alsa=no)
if test "x$have_alsa" = "xyes"  ; then
PA_LIBS+=-lasound
fi

AC_SUBST(PA_LIBS)

AM_CONDITIONAL([ENABLE_ZRTP],[test "x$enable_zrtp" != "xno"])

AM_CONDITIONAL([WANT_DEBUG],[test "${enable_debug}" = "yes"])

AC_ARG_ENABLE(core-odbc-support,
  [AS_HELP_STRING([--enable-core-odbc-support], [Compile with ODBC Support (default is optional)])],,[enable_core_odbc_support="optional"])
if ! test "$enable_core_odbc_support" = "no"; then
  AX_LIB_ODBC
  if test "$ac_cv_found_odbc" = "yes" ; then
    enable_core_odbc_support="yes"
  elif test "$enable_core_odbc_support" = "yes"; then
    AC_MSG_ERROR([no usable libodbc; please install unixodbc devel package or equivalent])
  else
    enable_core_odbc_support="no"
  fi
fi

CHECK_LIBUUID
SWITCH_AM_LDFLAGS="$LIBUUID_LIBS $SWITCH_AM_LDFLAGS"
SWITCH_AM_CFLAGS="$LIBUUID_CFLAGS $SWITCH_AM_CFLAGS"

AC_ARG_ENABLE(core-pgsql-support,     
	[AS_HELP_STRING([--enable-core-pgsql-support], [Compile with PGSQL Support])],,[enable_core_pgsql_support="no"])
AC_ARG_ENABLE(core-pgsql-pkgconfig,
	[AS_HELP_STRING([--disable-core-pgsql-pkgconfig], [Use pg_config to get PGQSL build options])],[enable_core_pgsql_pkgconfig="$enableval"],[enable_core_pgsql_pkgconfig="yes"])

if test x"$enable_core_pgsql_support" = x"yes" ; then
  AC_PATH_PROG([PG_CONFIG], [pg_config], [no])
  AC_PATH_PROG([PKG_CONFIG], [pkg-config], [no])
    if test "$PKG_CONFIG" = "no" \
       || test x"$enable_core_pgsql_pkgconfig" = x"no" \
       || ! pkg-config libpq; then
	if test "$PG_CONFIG" != "no"; then
            AC_MSG_CHECKING([for PostgreSQL libraries via pg_config])
            POSTGRESQL_CFLAGS="-I`$PG_CONFIG --includedir`"
            POSTGRESQL_LIBDIR="-L`$PG_CONFIG --libdir`"
            POSTGRESQL_LDFLAGS="-L`$PG_CONFIG --libdir` -lpq"
            POSTGRESQL_VERSION=`$PG_CONFIG --version | awk '{ print $NF }'`
            POSTGRESQL_MAJOR_VERSION=`$PG_CONFIG --version | awk '{ print $NF }' | awk -F. '{ print $1 }'`
            POSTGRESQL_MINOR_VERSION=`$PG_CONFIG --version | awk '{ print $NF }' | awk -F. '{ print $2 }'`
            POSTGRESQL_PATCH_VERSION=`$PG_CONFIG --version | awk '{ print $NF }' | awk -F. '{ print $3 }'`
        else
            AC_MSG_ERROR([no usable pkg-config or pg_config; please install libpq devel package or equivalent])
        fi
    else
      AC_MSG_CHECKING([for PostgreSQL libraries via pkg_config])
      POSTGRESQL_CFLAGS="`$PKG_CONFIG --cflags libpq`"
      POSTGRESQL_LIBDIR="`$PKG_CONFIG libpq --libs-only-L`"
      POSTGRESQL_LDFLAGS="`$PKG_CONFIG --libs libpq`"
      POSTGRESQL_VERSION="`$PKG_CONFIG --modversion libpq`"
      POSTGRESQL_MAJOR_VERSION="`echo $POSTGRESQL_VERSION | cut -d. -f1 | sed 's/^\([[0-9]]*\)[[^0-9]].*/\1/'`"
      POSTGRESQL_MINOR_VERSION="`echo $POSTGRESQL_VERSION | cut -d. -f2 | sed 's/^\([[0-9]]*\)[[^0-9]].*/\1/'`"
      POSTGRESQL_PATCH_VERSION="`echo $POSTGRESQL_VERSION | cut -d. -f3 | sed 's/^\([[0-9]]*\)[[^0-9]].*/\1/'`"
      test -n "$POSTGRESQL_PATCH_VERSION" || POSTGRESQL_PATCH_VERSION=0
    fi
    save_LIBS="$LIBS"
    LIBS="$POSTGRESQL_LIBDIR $LIBS"
    AC_MSG_RESULT([$POSTGRESQL_LIBDIR])

    AC_DEFINE([SWITCH_HAVE_PGSQL], [1], [Define to 1 if PostgreSQL libraries are available])
    AM_CONDITIONAL([HAVE_PGSQL],[true])
    AC_DEFINE_UNQUOTED([POSTGRESQL_VERSION], "${POSTGRESQL_VERSION}", [Specifies the version of PostgreSQL we are linking against])
    AC_DEFINE_UNQUOTED([POSTGRESQL_MAJOR_VERSION], ${POSTGRESQL_MAJOR_VERSION}, [Specifies the version of PostgreSQL we are linking against])
    AC_DEFINE_UNQUOTED([POSTGRESQL_MINOR_VERSION], ${POSTGRESQL_MINOR_VERSION}, [Specifies the version of PostgreSQL we are linking against])
    AC_DEFINE_UNQUOTED([POSTGRESQL_PATCH_VERSION], ${POSTGRESQL_PATCH_VERSION}, [Specifies the version of PostgreSQL we are linking against])

    AC_CHECK_LIB([pq], [PQgetvalue],, AC_MSG_ERROR([no usable libpq; please install PostgreSQL devel package or equivalent]))
    AC_MSG_RESULT([yes])
    SWITCH_AM_CXXFLAGS="$POSTGRESQL_CFLAGS $SWITCH_AM_CXXFLAGS"
    SWITCH_AM_LDFLAGS="$POSTGRESQL_LDFLAGS $SWITCH_AM_LDFLAGS"

    LIBS="$save_LIBS"
else
  AM_CONDITIONAL([HAVE_PGSQL],[false])
fi

AC_ARG_ENABLE(deprecated-core-db-events,
	[AS_HELP_STRING([--enable-deprecated-core-db-events], [Keep deprecated core db events])],,[enable_deprecated_core_db_events="no"])

if test x"$enable_deprecated_core_db_events" = x"yes" ; then
        AC_DEFINE([SWITCH_DEPRECATED_CORE_DB], [1], [Define to 1 to enable deprecated core db events])
fi

ESL_LDFLAGS=
PLATFORM_CORE_LDFLAGS=
PLATFORM_CORE_LIBS=

path_remove () {
  echo "$1" | tr ':' '\n' | grep -Fxv "$2" | tr '\n' ':' | sed 's/:$//'
}
path_push_unique () {
  x="$(eval echo \$$1)"
  x="$(path_remove "$x" "$2")"
  if test -z "$x"; then
    eval export $1="$2"
  else
    eval export $1="$2:$x"
  fi
}

# tweak platform specific flags
case "$host" in
    *darwin*)
        # Common Apple Darwin settings
        APR_ADDTO(SWITCH_AM_CFLAGS, -DMACOSX)
        APR_REMOVEFROM(SWITCH_AM_CFLAGS, -fPIC)
        APR_ADDTO(CPPFLAGS, -I/usr/local/opt/openssl/include)
        APR_ADDTO(LDFLAGS, -L/usr/local/opt/openssl/lib)
        if test "x$enable_core_odbc_support" != "xno"; then
	   APR_ADDTO([PLATFORM_CORE_LDFLAGS], [--framework CoreFoundation])
	fi
	APR_ADDTO([PLATFORM_CORE_LIBS], [-ldl])
    # Get OSX and clang version
        osxvrm=`sw_vers -productVersion`          # Get version.release.modlevel
	osxrel=`echo $osxvrm | cut -d. -f2`       # Get release only
        clangvers="`clang -v 2>&1  >/dev/null | grep version | sed -e 's/.*version \([[0-9]]*\).*$/\1/'`"
        if test "$clangvers" -ge 6; then          # Xcode 6 drops std lib search, add it to clang
           APR_ADDTO(LDFLAGS, -L/usr/local/lib)
           APR_ADDTO(CPPFLAGS, -I/usr/local/include)
        fi
        if test "$clangvers" -ge 4; then          # Xcode 4 / 10.7 and up
           APR_ADDTO(CFLAGS, -Wno-deprecated-declarations)
        fi
        if test "$osxrel" -ge 6; then             # 10.6 and up
           APR_ADDTO(CFLAGS, -pipe -no-cpp-precomp)
           APR_ADDTO(LDFLAGS, -pipe -bind_at_load)
           APR_ADDTO(CXXFLAGS, -pipe)
        fi
    ;;
    *-solaris2*)
	if test "${enable_64}" = "yes"; then
           APR_ADDTO(CPPFLAGS, [-I/opt/64/include])
           APR_ADDTO(LDFLAGS, [-L/opt/64/lib -Wl,-rpath,/opt/64/lib])
           APR_ADDTO(SWITCH_AM_CFLAGS, [-I/opt/64/include])
           APR_ADDTO(SWITCH_AM_LDFLAGS, [-L/opt/64/lib -Wl,-rpath,/opt/64/lib])
	else
           APR_ADDTO(CPPFLAGS, [-I/opt/include])
           APR_ADDTO(LDFLAGS, [-L/opt/lib -Wl,-rpath,/opt/lib])
           APR_ADDTO(SWITCH_AM_CFLAGS, [-I/opt/include])
           APR_ADDTO(SWITCH_AM_LDFLAGS, [-L/opt/lib -Wl,-rpath,/opt/lib])
	fi
        APR_ADDTO(SWITCH_AM_CFLAGS, -DPATH_MAX=2048 -D__EXTENSIONS__)
        APR_ADDTO(SWITCH_AM_LDFLAGS, -lsendfile -lresolv -lsocket -lnsl -luuid)
        APR_ADDTO(ESL_LDFLAGS, -lnsl -lsocket)
	APR_ADDTO([PLATFORM_CORE_LIBS], [-ldl -lcrypt -lrt -lsendfile -lresolv -lsocket -lnsl -luuid])
    ;;
    *dragonfly*)
        APR_ADDTO(CPPFLAGS, -I/usr/local/include)
        APR_ADDTO(LDFLAGS, -L/usr/local/lib)
        APR_ADDTO(SWITCH_AM_CFLAGS, -I/usr/local/include)
    ;;
    *openbsd*)
        APR_ADDTO(CPPFLAGS, -I/usr/local/include)
        APR_ADDTO(LDFLAGS, -L/usr/local/lib)
        APR_ADDTO(SWITCH_AM_CFLAGS, -I/usr/local/include)
    ;;
    *netbsd*)
        APR_ADDTO(CPPFLAGS, -I/usr/pkg/include)
        APR_ADDTO(LDFLAGS, [-L/usr/pkg/lib -Wl,-rpath,/usr/pkg/lib])
        APR_ADDTO(SWITCH_AM_CFLAGS, -I/usr/pkg/include)
    ;;
    *bsd*)
        APR_ADDTO(CPPFLAGS, -I/usr/local/include)
        APR_ADDTO(LDFLAGS, -L/usr/local/lib)
        APR_ADDTO(SWITCH_AM_CFLAGS, -I/usr/local/include)
	APR_ADDTO([PLATFORM_CORE_LIBS], [-lcrypt -lrt])
    ;;
    *linux*)
	APR_ADDTO([PLATFORM_CORE_LIBS], [-ldl -lcrypt -lrt])
    ;;
esac

APR_REMOVEFROM(SWITCH_AM_CXXFLAGS, -std=c99)

AC_CHECK_LIB(z, inflateReset, have_libz=yes, AC_MSG_ERROR([no usable zlib; please install zlib devel package or equivalent]))
if test "x$have_libz" = "xyes"  ; then
APR_ADDTO([PLATFORM_CORE_LIBS], [-lz])
fi

AC_CHECK_LIB(apr-1, apr_pool_mutex_set, use_system_apr=yes, use_system_apr=no)
AM_CONDITIONAL([SYSTEM_APR],[test "${use_system_apr}" = "yes"])
AC_CHECK_LIB(aprutil-1, apr_queue_pop_timeout, use_system_aprutil=yes, use_system_aprutil=no)
AM_CONDITIONAL([SYSTEM_APRUTIL],[test "${use_system_aprutil}" = "yes"])

save_LIBS="$LIBS"
LIBS=
AC_CHECK_LIB(jpeg, jpeg_std_error,, AC_MSG_ERROR([no usable libjpeg; please install libjpeg devel package or equivalent]))

AC_CHECK_LIB(jbig, jbg_enc_out, have_libjbig=yes, have_libjbig=no)
if test "x$have_libjbig" = "xyes"  ; then
SPANDSP_LA_JBIG="-ljbig $LIBS"
AC_SUBST(SPANDSP_LA_JBIG)
fi
LIBS="$save_LIBS"

AC_CHECK_LIB(lzma, lzma_code, have_liblzma=yes, have_liblzma=no)
if test "x$have_liblzma" = "xyes" ; then
SPANDSP_LA_LZMA="-llzma"
AC_SUBST(SPANDSP_LA_LZMA)
fi

AC_CHECK_LIB(resolv, res_init, have_libresolv=yes, have_libresolv=no)
if test "x$have_libresolv" = "xyes"  ; then
APR_ADDTO(SWITCH_AM_LDFLAGS, -lresolv)
fi

AC_SUBST(SWITCH_AM_CFLAGS)
AC_SUBST(SWITCH_ANSI_CFLAGS)
AC_SUBST(SWITCH_AM_CXXFLAGS)
AC_SUBST(SWITCH_AM_LDFLAGS)
AC_SUBST(ESL_LDFLAGS)
AC_SUBST(PLATFORM_CORE_LDFLAGS)
AC_SUBST(PLATFORM_CORE_LIBS)
AC_SUBST(SOLINK)
AC_SUBST(LIBTOOL_LIB_EXTEN)

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([sys/types.h sys/resource.h sched.h wchar.h sys/filio.h sys/ioctl.h sys/prctl.h sys/select.h netdb.h execinfo.h])

if test x"$ac_cv_header_wchar_h" = xyes; then
  HAVE_WCHAR_H_DEFINE=1
else
  HAVE_WCHAR_H_DEFINE=0
fi
AC_SUBST(HAVE_WCHAR_H_DEFINE)

# Needed by Abyss on Solaris:

if test x"$ac_cv_header_sys_filio_h" = xyes; then
  HAVE_SYS_FILIO_H_DEFINE=1
else
  HAVE_SYS_FILIO_H_DEFINE=0
fi
AC_SUBST(HAVE_SYS_FILIO_H_DEFINE)

# Needed by Abyss on Solaris:

if test x"$ac_cv_header_sys_ioctl_h" = xyes; then
  HAVE_SYS_IOCTL_H_DEFINE=1
else
  HAVE_SYS_IOCTL_H_DEFINE=0
fi
AC_SUBST(HAVE_SYS_IOCTL_H_DEFINE)

if test x"$ac_cv_header_sys_select_h" = xyes; then
  HAVE_SYS_SELECT_H_DEFINE=1
else
  HAVE_SYS_SELECT_H_DEFINE=0
fi
AC_SUBST(HAVE_SYS_SELECT_H_DEFINE)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_TYPE_SIGNAL
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([gethostname vasprintf mmap mlock mlockall usleep getifaddrs timerfd_create getdtablesize posix_openpt poll])
AC_CHECK_FUNCS([sched_setscheduler setpriority setrlimit setgroups initgroups])
AC_CHECK_FUNCS([wcsncmp setgroups asprintf setenv pselect gettimeofday localtime_r gmtime_r strcasecmp stricmp _stricmp])

# Check availability and return type of strerror_r
# (NOTE: apr-1-config sets -D_GNU_SOURCE at build-time, need to run the check with it too)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
AC_FUNC_STRERROR_R
CPPFLAGS="$save_CPPFLAGS"

AX_HAVE_CPU_SET

AC_CHECK_LIB(rt, clock_gettime, [AC_DEFINE(HAVE_CLOCK_GETTIME, 1, [Define if you have clock_gettime()])])
AC_CHECK_LIB(rt, clock_getres, [AC_DEFINE(HAVE_CLOCK_GETRES, 1, [Define if you have clock_getres()])])
AC_CHECK_LIB(rt, clock_nanosleep, [AC_DEFINE(HAVE_CLOCK_NANOSLEEP, 1, [Define if you have clock_nanosleep()])])
AC_CHECK_LIB(pthread, pthread_setschedparam, [AC_DEFINE(HAVE_PTHREAD_SETSCHEDPARAM, 1, [Define if you have pthread_setschedparam()])])

AC_CHECK_FUNC(socket, , AC_CHECK_LIB(socket, socket))

AC_CHECK_FILE(/dev/ptmx, [AC_DEFINE(HAVE_DEV_PTMX, 1, [Define if you have /dev/ptmx])])
AC_CHECK_LIB(util, openpty, [AC_DEFINE(HAVE_OPENPTY, 1, [Define if you have openpty()])])

AC_CHECK_MEMBERS([struct tm.tm_gmtoff],,,[
#include <sys/types.h>
#include <time.h>])

AC_CHECK_MEMBERS([struct tm.tm_zone],,,[
#include <sys/types.h>
#include <time.h>])

AC_CHECK_DECL([RLIMIT_MEMLOCK],
	[AC_DEFINE([HAVE_RLIMIT_MEMLOCK],[1],[RLIMIT_MEMLOCK constant for setrlimit])],,
	[#ifdef HAVE_SYS_RESOURCE_H
	 #include <sys/resource.h>
	 #endif])

AC_CHECK_DECL([SCHED_RR],
	[AC_DEFINE([HAVE_SCHED_RR],[1],[SCHED_RR constant for sched_setscheduler])],,
	[#ifdef HAVE_SCHED_H
	 #include <sched.h>
	 #endif])

AC_CHECK_DECL([SCHED_FIFO],
	[AC_DEFINE([HAVE_SCHED_FIFO],[1],[SCHED_FIFO constant for sched_setscheduler])],,
	[#ifdef HAVE_SCHED_H
	 #include <sched.h>
	 #endif])

#
# use mlockall only on linux (for now; if available)
#
if test "x${ac_cv_func_mlockall}" = "xyes"; then
	AC_MSG_CHECKING([whether to use mlockall])
	case "$host" in
	*-linux-*)
		AC_DEFINE([USE_MLOCKALL],[1],[Enable mlockall support])
		AC_MSG_RESULT([yes])
		USE_MLOCKALL=yes
		;;
	*-freebsd*)
		APR_ADDTO(SWITCH_AM_CFLAGS, -fPIC)
		APR_ADDTO(SWITCH_AM_CXXFLAGS, -fPIC)
		AC_MSG_RESULT([no, broken for non-root users])
		;;
	*)
		AC_MSG_RESULT([no])
		;;
	esac

	#
	# setrlimit prerequisites
	#
	if test "x${USE_MLOCKALL}" = "xyes" -a \
		"x${ac_cv_func_setrlimit}" = "xyes" -a \
		"x${ac_cv_have_decl_RLIMIT_MEMLOCK}" = "xyes"
	then
		AC_DEFINE([USE_SETRLIMIT],[1],[Use setrlimit to disable mlock limit for non-root users])
	fi
fi

#
# sched_setcheduler + round-robin scheduler prerequisites
#
if test "x${ac_cv_func_sched_setscheduler}" = "xyes" -a \
        "x${ac_cv_have_decl_SCHED_RR}" = "xyes"
then
	AC_DEFINE([USE_SCHED_SETSCHEDULER],[1],[Enable round-robin scheduler using sched_setscheduler])
fi
#
# xmlrpc-c checks
#

AC_CHECK_FUNCS(setenv strtoll strtoull strtoq strtouq __strtoll __strtoull)

HAVE_LIBWWW_SSL_DEFINE=0
AC_SUBST(HAVE_LIBWWW_SSL_DEFINE)

DIRECTORY_SEPARATOR="/"
AC_SUBST(DIRECTORY_SEPARATOR)

va_list_is_array=no
AC_MSG_CHECKING(whether va_list is an array)
AC_TRY_COMPILE([
#include <stdarg.h>
], [va_list list1, list2; list1 = list2;], , 
va_list_is_array=yes)
AC_MSG_RESULT($va_list_is_array)
if test x"$va_list_is_array" = xyes; then
  VA_LIST_IS_ARRAY_DEFINE=1
else
  VA_LIST_IS_ARRAY_DEFINE=0
fi
AC_SUBST(VA_LIST_IS_ARRAY_DEFINE)


AC_MSG_CHECKING(whether compiler has __attribute__)
AC_TRY_COMPILE(, [int x __attribute__((__unused__));],
compiler_has_attribute=yes,
compiler_has_attribute=no)
AC_MSG_RESULT($compiler_has_attribute)
if test x"$compiler_has_attribute" = xyes; then
    ATTR_UNUSED="__attribute__((__unused__))"
else
    ATTR_UNUSED=
fi
AC_SUBST(ATTR_UNUSED)


saved_CFLAGS="$CFLAGS"
AC_CACHE_CHECK([whether compiler supports -Wdeclaration-after-statement], [ac_cv_gcc_declaration_after_statement], [
CFLAGS="$CFLAGS -Wdeclaration-after-statement"
AC_TRY_COMPILE([],[return 0;],[ac_cv_gcc_declaration_after_statement=yes],[ac_cv_gcc_declaration_after_statement=no])
])
AC_MSG_RESULT($ac_cv_gcc_declaration_after_statement)
if test x"$ac_cv_gcc_declaration_after_statement" = xyes; then
    APR_ADDTO(SWITCH_ANSI_CFLAGS, -Wdeclaration-after-statement)
fi
CFLAGS="$saved_CFLAGS"

if test "x${ax_cv_c_compiler_vendor}" = "xclang" ; then
     # Next check added for Xcode 5 and systems with clang 5 llvm 3.3 or above, extended offset must be off
     AC_CACHE_CHECK([whether compiler supports -Wextended-offsetof], [ac_cv_clang_extended_offsetof], [
       AC_TRY_COMPILE([],[return 0;],[ac_cv_clang_extended_offsetof=yes],[ac_cv_clang_extended_offsetof=no])
     ])
     AC_MSG_RESULT($ac_cv_clang_extended_offsetof)
     if test x"$ac_cv_clang_extended_offsetof" = xyes; then
         APR_ADDTO(CFLAGS, -Wno-extended-offsetof)
     fi
fi

# Tested and fixed lot of modules, but some are untested.  Will be added back when the core team decide it ready
# Untested modules : mod_osp mod_soundtouch mod_sangoma_codec mod_dingaling mod_opal mod_skypopen mod_h323 mod_khomp 
#                    mod_unimrcp mod_cepstral mod_erlang_event mod_snmp mod_perl mod_java mod_managed
#
#saved_CFLAGS="$CFLAGS"
#AC_CACHE_CHECK([whether compiler supports -Wunused-but-set-variable], [ac_cv_gcc_unused_but_set_variable], [
#CFLAGS="$CFLAGS -Wunused-but-set-variable"
#AC_TRY_COMPILE([],[return 0;],[ac_cv_gcc_unused_but_set_variable=yes],[ac_cv_gcc_unused_but_set_variable=no])
#])
#AC_MSG_RESULT($ac_cv_gcc_unused_but_set_variable)
#if test x"$ac_cv_gcc_unused_but_set_variable" = xyes; then
#    APR_ADDTO(SWITCH_ANSI_CFLAGS, -Wunused-but-set-variable)
#fi
#CFLAGS="$saved_CFLAGS"

AC_C_BIGENDIAN(AC_DEFINE([SWITCH_BYTE_ORDER],__BIG_ENDIAN,[Big Endian]),AC_DEFINE([SWITCH_BYTE_ORDER],__LITTLE_ENDIAN,[Little Endian]))

# Checks for integer size
AC_CHECK_SIZEOF(char, 1)
AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(long, 4)
AC_CHECK_SIZEOF(short, 2)
AC_CHECK_SIZEOF(long long, 8)
AC_TYPE_SIZE_T
AC_CHECK_TYPE(ssize_t, int)

# Checks for pointer size
AC_CHECK_SIZEOF(void*, 4)

if test "x$ac_cv_sizeof_voidp" != "x"; then
    voidp_size=$ac_cv_sizeof_voidp
else
    AC_ERROR([Cannot determine size of void*])
fi

if test "$ac_cv_sizeof_short" = "2"; then
    short_value=short
fi
if test "$ac_cv_sizeof_int" = "4"; then
    int_value=int
fi

if test "$ac_cv_sizeof_int" = "8"; then
    int64_t_fmt='#define SWITCH_INT64_T_FMT "d"'
    uint64_t_fmt='#define SWITCH_UINT64_T_FMT "u"'
    int64_value="int"
    long_value=int
elif test "$ac_cv_sizeof_long" = "8"; then
    int64_t_fmt='#define SWITCH_INT64_T_FMT "ld"'
    uint64_t_fmt='#define SWITCH_UINT64_T_FMT "lu"'
    int64_value="long"
    long_value=long
    case "$host" in
        *pc-solaris2*)
        ;;
        *-solaris2*|*apple-darwin*|*-openbsd*)
	    if test "$ac_cv_sizeof_long_long" = "8"; then
	        int64_t_fmt='#define SWITCH_INT64_T_FMT "lld"'
		uint64_t_fmt='#define SWITCH_UINT64_T_FMT "llu"'
		int64_value="long long"
		long_value="long long"
	    fi
	;;
    esac
elif test "$ac_cv_sizeof_long_long" = "8"; then
    int64_t_fmt='#define SWITCH_INT64_T_FMT "lld"'
    uint64_t_fmt='#define SWITCH_UINT64_T_FMT "llu"'
    int64_value="long long"
    long_value="long long"
elif test "$ac_cv_sizeof_longlong" = "8"; then
    int64_t_fmt='#define SWITCH_INT64_T_FMT "qd"'
    uint64_t_fmt='#define SWITCH_UINT64_T_FMT "qu"'
    int64_value="__int64"
    long_value="__int64"
else
    AC_ERROR([could not detect a 64-bit integer type])
fi

if test "$ac_cv_type_size_t" = "yes"; then
    size_t_value="size_t"
else
    size_t_value="switch_int32_t"
fi

if test "$ac_cv_type_ssize_t" = "yes"; then
    ssize_t_value="ssize_t"
else
    ssize_t_value="switch_int32_t"
fi

APR_CHECK_SIZEOF_EXTENDED([#include <sys/types.h>], ssize_t, 8)

if test "$ac_cv_sizeof_ssize_t" = "$ac_cv_sizeof_int"; then
    ssize_t_fmt='#define SWITCH_SSIZE_T_FMT "d"'
elif test "$ac_cv_sizeof_ssize_t" = "$ac_cv_sizeof_long"; then
    ssize_t_fmt='#define SWITCH_SSIZE_T_FMT "ld"'
else
    ssize_t_fmt='#error Can not determine the proper size for ssize_t'
fi

APR_CHECK_SIZEOF_EXTENDED([#include <stddef.h>], size_t, 8)

if test "$ac_cv_sizeof_size_t" = "$ac_cv_sizeof_int"; then
    size_t_fmt='#define SWITCH_SIZE_T_FMT "d"'
elif test "$ac_cv_sizeof_size_t" = "$ac_cv_sizeof_long"; then
    size_t_fmt='#define SWITCH_SIZE_T_FMT "ld"'
else
    size_t_fmt='#define SWITCH_SIZE_T_FMT "zu"'
fi

# Basically, we have tried to figure out the correct format strings
# for SWITCH types which vary between platforms, but we don't always get 
# it right.  If you find that we don't get it right for your platform, 
# you can override our decision below.
# NOTE: borrowed much of this logic from apr.
case $host in
   s390*linux*)
       # uniquely, the 31-bit Linux/s390 uses "unsigned long int"
       # for size_t rather than "unsigned int":
       size_t_fmt='#define SWITCH_SIZE_T_FMT "lu"'
       ssize_t_fmt='#define SWITCH_SSIZE_T_FMT "ld"'
       ;;
   *-os2*)
       size_t_fmt='#define SWITCH_SIZE_T_FMT "lu"'
       ;;
   *-openbsd*)
       size_t_fmt='#define SWITCH_SIZE_T_FMT "ld"'
       ;;
   *aix4*|*aix5*)
       ssize_t_fmt='#define SWITCH_SSIZE_T_FMT "ld"'
       size_t_fmt='#define SWITCH_SIZE_T_FMT "ld"'
       ;;
    *beos*)
        ssize_t_fmt='#define SWITCH_SSIZE_T_FMT "ld"'
        size_t_fmt='#define SWITCH_SIZE_T_FMT "ld"'
        ;;
    *apple-darwin*)
         ssize_t_fmt='#define SWITCH_SSIZE_T_FMT "ld"'
         size_t_fmt='#define SWITCH_SIZE_T_FMT "lu"'
        ;;
esac

AC_SUBST(voidp_size)
AC_SUBST(short_value)
AC_SUBST(int_value)
AC_SUBST(long_value)
AC_SUBST(int64_value)
AC_SUBST(size_t_value)
AC_SUBST(ssize_t_value)
AC_SUBST(int64_t_fmt) 
AC_SUBST(uint64_t_fmt) 
AC_SUBST(ssize_t_fmt) 
AC_SUBST(size_t_fmt)

case $host in
   *-openbsd*)
       # OpenBSD's gunzip and friends don't like -d because its redundant, only gzip does
       AC_PATH_PROGS(ZCAT, gzip)
       ;;
    *)
       AC_PATH_PROGS(ZCAT, gunzip gzcat gzip zcat)
       ;;
esac

AC_PATH_PROGS(BZIP, bzip2)
AC_PATH_PROGS(XZ, xz)
AC_PATH_PROGS(TAR, gtar tar)
AC_PATH_PROGS(WGET, wget)
AC_PATH_PROGS(CURL, curl)
GETLIB="cd $switch_srcdir/libs && ${SHELL} $switch_builddir/build/getlib.sh"
AC_SUBST(GETLIB)
GETG729="cd $switch_srcdir/libs && ${SHELL} $switch_builddir/build/getg729.sh"
AC_SUBST(GETG729)
GETSOUNDS="${SHELL} $switch_builddir/build/getsounds.sh"
AC_SUBST(GETSOUNDS)

case $host in
   *-darwin*)
        path_push_unique PKG_CONFIG_PATH /usr/local/opt/curl/lib/pkgconfig
        path_push_unique PKG_CONFIG_PATH /usr/local/opt/sqlite/lib/pkgconfig/
        path_push_unique PKG_CONFIG_PATH /usr/local/opt/ldns/lib/pkgconfig/
        path_push_unique PKG_CONFIG_PATH /usr/local/opt/portaudio/lib/pkgconfig/
       ;;
esac

if ! (test -x "$PKG_CONFIG" || test -x "$(which pkg-config)"); then
  AC_MSG_ERROR([You need to install pkg-config to configure FreeSWITCH.])
fi

# temporary workaround for Debian libldns-dev package bug
if test "$cross_compiling" != "yes" && test -f /usr/lib/pkg-config/libldns.pc; then
  path_push_unique PKG_CONFIG_PATH /usr/lib/pkg-config
fi

module_enabled() {
  grep -v -e "\#" -e "^\$" modules.conf | sed -e "s|^.*/||" | grep "^${1}\$" >/dev/null
}

PKG_CHECK_MODULES([SQLITE], [sqlite3 >= 3.6.20])
PKG_CHECK_MODULES([CURL], [libcurl >= 7.19])
PKG_CHECK_MODULES([PCRE], [libpcre >= 7.8])
PKG_CHECK_MODULES([SPEEX], [speex >= 1.2rc1 speexdsp >= 1.2rc1])
PKG_CHECK_MODULES([YAML], [yaml-0.1 >= 0.1.4],[
  AM_CONDITIONAL([HAVE_YAML],[true])],[
  AC_MSG_RESULT([no]); AM_CONDITIONAL([HAVE_YAML],[false])])
PKG_CHECK_MODULES([PORTAUDIO], [portaudio-2.0 >= 19],[
  AM_CONDITIONAL([HAVE_PORTAUDIO],[true])],[
  AC_MSG_RESULT([no]); AM_CONDITIONAL([HAVE_PORTAUDIO],[false])])
PKG_CHECK_MODULES([LDNS], [libldns >= 1.6.6],[
  AM_CONDITIONAL([HAVE_LDNS],[true])],[
  AC_CHECK_LIB([ldns], [ldns_str2rdf_a], [LDNS_LIBS=-lldns])
  AS_IF([test -z "$LDNS_LIBS"],[
    if module_enabled mod_enum; then
      AC_MSG_ERROR([You need to either install libldns-dev or disable mod_enum in modules.conf])
    else
      AC_MSG_RESULT([no]); AM_CONDITIONAL([HAVE_LDNS],[false])
    fi],[
    AM_CONDITIONAL([HAVE_LDNS],[true])])])

PKG_CHECK_MODULES([MEMCACHED], [libmemcached >= 0.31],[
  AM_CONDITIONAL([HAVE_MEMCACHED],[true])
  MEMCACHED_LIBS="${MEMCACHED_LIBS} -lpthread"
  save_LIBS="${LIBS}"
  save_CPPFLAGS="${CPPFLAGS}"
  LIBS="${MEMCACHED_LIBS}"
  CPPFLAGS="${MEMCACHED_CFLAGS}"
  AC_CHECK_FUNCS([memcached_server_name memcached_stat_execute])
  AC_CHECK_TYPES([memcached_instance_st*],,, [[#include <libmemcached/memcached.h>]])
  LIBS="${save_LIBS}"
  CPPFLAGS="${save_CPPFLAGS}"
],[
  AC_MSG_RESULT([no])
  AM_CONDITIONAL([HAVE_MEMCACHED],[false])
])

PKG_CHECK_MODULES([AMQP], [librabbitmq >= 0.5.2],[
  AM_CONDITIONAL([HAVE_AMQP],[true])],[
  AC_MSG_RESULT([no]); AM_CONDITIONAL([HAVE_AMQP],[false])])

AC_ARG_ENABLE(core-libedit-support,     
  [AS_HELP_STRING([--disable-core-libedit-support], [Compile without libedit Support])])

AS_IF([test "x$enable_core_libedit_support" != "xno"],[
  PKG_CHECK_MODULES([LIBEDIT], [libedit >= 2.11],,[
    AC_MSG_RESULT([no])
    AC_CHECK_LIB([edit], [el_line], [LIBEDIT_LIBS=-ledit])
    AC_CHECK_HEADER([histedit.h], [], [unset LIBEDIT_LIBS])
    AS_IF([test "x$LIBEDIT_LIBS" = "x"], [
      AC_MSG_ERROR([You need to either install libedit-dev (>= 2.11) or configure with --disable-core-libedit-support])
      ])])])

AS_IF([test "x$enable_core_libedit_support" != "xno"], [
  # If making changes here, don't forget to run autoheader and
  # update libs/esl/src/include/esl_config_auto.h.in manually.
  AC_DEFINE([HAVE_LIBEDIT], [1], [Define to 1 if you have libedit is available])
  save_LIBS="${LIBS}"
  save_CPPFLAGS="${CPPFLAGS}"
  LIBS="${LIBEDIT_LIBS}"
  CPPFLAGS="${LIBEDIT_CFLAGS}"
  AC_CHECK_DECLS([EL_PROMPT_ESC, EL_REFRESH],,, [[#include <histedit.h>]])
  AC_CHECK_FUNCS([el_wset])
  LIBS="${save_LIBS}"
  CPPFLAGS="${save_CPPFLAGS}"
])

SAC_OPENSSL

if test x$HAVE_OPENSSL = x1; then
	openssl_CFLAGS="$openssl_CFLAGS -DHAVE_OPENSSL";
	APR_ADDTO(SWITCH_AM_CFLAGS, -DHAVE_OPENSSL)
	AC_CHECK_LIB(ssl, SSL_CTX_set_tlsext_use_srtp, AC_DEFINE_UNQUOTED(HAVE_OPENSSL_DTLS_SRTP, 1, HAVE_OPENSSL_DTLS_SRTP), AC_MSG_ERROR([OpenSSL >= 1.0.1e and associated developement headers required]))
	AC_CHECK_LIB(ssl, DTLSv1_method, AC_DEFINE_UNQUOTED(HAVE_OPENSSL_DTLS, 1, HAVE_OPENSSL_DTLS), AC_MSG_ERROR([OpenSSL >= 1.0.1e and associaed developement headers required]))
else
	AC_MSG_ERROR([OpenSSL >= 1.0.1e and associated developement headers required])
fi

AX_CHECK_JAVA

# Option to enable static linking of Google's V8 inside mod_v8
AC_ARG_ENABLE(static-v8,
[AS_HELP_STRING([--enable-static-v8], [Statically link V8 into mod_v8])], [enable_static_v8="$enableval"], [enable_static_v8="no"])
AM_CONDITIONAL([ENABLE_STATIC_V8],[test "x$enable_static_v8" != "xno"])

# Option to disable parallel build of Google's V8
AC_ARG_ENABLE(parallel-build-v8,
[AS_HELP_STRING([--disable-parallel-build-v8], [Disable parallel build of V8])], [enable_parallel_build_v8="$enableval"], [enable_parallel_build_v8="yes"])
AM_CONDITIONAL([ENABLE_PARALLEL_BUILD_V8],[test "x$enable_parallel_build_v8" != "xno"])

AM_CONDITIONAL([HAVE_ODBC],[test "x$enable_core_odbc_support" != "xno"])
AM_CONDITIONAL([HAVE_MYSQL],[test "$found_mysql" = "yes"])

#
# perl checks
#

AC_CHECK_PROG(PERL,perl,[ac_cv_have_perl=yes],[ac_cv_have_perl=no])

# -a "x$ac_cv_have_EXTERN_h" != "xno"

if test "x$ac_cv_have_perl" != "xno"; then
   PERL=perl
   PERL_SITEDIR="`$PERL -MConfig -e 'print $Config{archlib}'`"
   PERL_LIBDIR="-L`$PERL -MConfig -e 'print $Config{archlib}'`/CORE"
   PERL_LIBS="`$PERL -MConfig -e 'print $Config{libs}'`"
   PERL_CFLAGS="-w -DMULTIPLICITY `$PERL -MExtUtils::Embed -e ccopts | sed -e 's|-arch x86_64 -arch i386||'` -DEMBED_PERL"
   PERL_LDFLAGS="`$PERL -MExtUtils::Embed -e ldopts| sed -e 's|-arch x86_64 -arch i386||'`"
   PERL_INC="`$PERL -MExtUtils::Embed -e perl_inc`"

   save_CFLAGS="$CFLAGS"
   CFLAGS="$PERL_CFLAGS"
   AC_CHECK_HEADER([EXTERN.h], [ac_cv_have_EXTERN_h=yes], [ac_cv_have_EXTERN_h=no], [[#include <EXTERN.h>
# include <perl.h>]])
   CFLAGS="$save_CFLAGS"

   save_LDFLAGS="$LDFLAGS"
   LDFLAGS="$PERL_LDFLAGS"
   AC_CHECK_LIB([perl], [perl_alloc], ac_cv_use_libperl=yes, ac_cv_use_libperl=no)
   LDFLAGS="$save_LDFLAGS"

   AC_SUBST(PERL_SITEDIR)
   AC_SUBST(PERL_LIBDIR)
   AC_SUBST(PERL_LIBS)
   AC_SUBST(PERL_CFLAGS)
   AC_SUBST(PERL_LDFLAGS)
   AC_SUBST(PERL_INC)
fi

AM_CONDITIONAL([HAVE_PERL],[test "x$ac_cv_have_perl" != "xno" -a "x$ac_cv_have_EXTERN_h" != "xno" -a "x$ac_cv_use_libperl" != "xno"])

#
# php checks
#

AC_CHECK_PROG(PHP,php,[ac_cv_have_php=yes],[ac_cv_have_php=no])
AC_CHECK_PROG(PHP_CONFIG,php-config,[ac_cv_have_php_config=yes],[ac_cv_have_php_config=no])
AM_CONDITIONAL([HAVE_PHP],[test "x$ac_cv_have_php" != "xno" -a "x$ac_cv_have_php_config" != "xno"])

if test "x$ac_cv_have_php" != "xno" -a "x$ac_cv_have_php_config" != "xno"; then
   PHP=php
   PHP_CONFIG=php-config
   PHP_LDFLAGS="`$PHP_CONFIG --ldflags`"
   PHP_LIBS="`$PHP_CONFIG --libs | sed -r 's/ ?-l(bz2|pcre|xml2|gssapi_krb5|krb5|k5crypto|com_err|history|z|readline|gmp|ssl|crypto)//g'`"
   PHP_EXT_DIR="`$PHP_CONFIG --extension-dir`"
   PHP_INC_DIR="`$PHP -r 'echo ini_get("include_path");' | cut -d: -f2`"
   PHP_INI_DIR="`$PHP_CONFIG --configure-options | tr " " "\n" | grep -- --with-config-file-scan-dir | cut -f2 -d=`"
   PHP_CFLAGS="`$PHP_CONFIG --includes`"
   AC_SUBST(PHP_LDFLAGS)
   AC_SUBST(PHP_LIBS)
   AC_SUBST(PHP_EXT_DIR)
   AC_SUBST(PHP_INC_DIR)
   AC_SUBST(PHP_INI_DIR)
   AC_SUBST(PHP_CFLAGS)
fi

#
# Python checks for mod_python
#
AC_ARG_WITH(
	[python],
	[AS_HELP_STRING([--with-python], [Use system provided version of python (default: try)])],
	[with_python="$withval"],
	[with_python="try"]
)

if test "$with_python" != "no"
then
	save_CFLAGS="$CFLAGS"
	save_LIBS="$LIBS"

	if test "$with_python" != "yes" -a "$with_python" != "try" ; then
		AC_MSG_CHECKING([for python])
		if test ! -x "$with_python" ; then
			AC_MSG_ERROR([Specified python does not exist or is not executable: $with_python])
		fi
		AC_MSG_RESULT([$with_python])
		AC_SUBST([PYTHON], ["$with_python"])
	else
		AC_PATH_PROG([PYTHON], ["python"], ["no"], ["$PATH:/usr/bin:/usr/local/bin"])
	fi

	if test "$PYTHON" != "no" ; then
		AC_MSG_CHECKING([python version])
		PYTHON_VER="`$PYTHON -V 2>&1 | cut -d' ' -f2`"

		if test -z "$PYTHON_VER" ; then
			AC_MSG_ERROR([Unable to detect python version])
		fi
		AC_MSG_RESULT([$PYTHON_VER])

		AC_MSG_CHECKING([for python distutils])
		python_result="`$PYTHON -c 'import distutils;' 2>&1`"
		if test -z "$python_result" ; then
			python_has_distutils="yes"
		else
			python_has_distutils="no"
		fi
		AC_MSG_RESULT([$python_has_distutils])

		if test "$python_has_distutils" != "no" ; then
			AC_MSG_CHECKING([location of site-packages])

			PYTHON_SITE_DIR="`$PYTHON -c 'from distutils import sysconfig; print(sysconfig.get_python_lib(0));'`"

			if test -z "$PYTHON_SITE_DIR" ; then
				AC_MSG_ERROR([Unable to detect python site-packages path])
			elif test ! -d "$PYTHON_SITE_DIR" ; then
				AC_MSG_ERROR([Path $PYTHON_SITE_DIR returned by python does not exist!])
			fi
			AC_MSG_RESULT([$PYTHON_SITE_DIR])
			AC_SUBST([PYTHON_SITE_DIR], [$PYTHON_SITE_DIR])

			#
			# python distutils found, get settings from python directly
			#
			PYTHON_CFLAGS="`$PYTHON -c 'from distutils import sysconfig; flags = [[\"-I\" + sysconfig.get_python_inc(0), \"-I\" + sysconfig.get_python_inc(1), \" \".join(sysconfig.get_config_var(\"CFLAGS\").split())]]; print(\" \".join(flags));' | sed -e 's/-arch i386//g;s/-arch x86_64//g'`"
			PYTHON_LDFLAGS="`$PYTHON -c 'from distutils import sysconfig; libs = sysconfig.get_config_var(\"LIBS\").split() + sysconfig.get_config_var(\"SYSLIBS\").split(); libs.append(\"-lpython\"+sysconfig.get_config_var(\"VERSION\")); print(\" \".join(libs));'`"
			PYTHON_LIB="`$PYTHON -c 'from distutils import sysconfig; print(\"python\" + sysconfig.get_config_var(\"VERSION\"));'`"
			PYTHON_LIBDIR="`$PYTHON -c 'from distutils import sysconfig; print(sysconfig.get_config_var(\"LIBDIR\"));'`"

			# handle python being installed into /usr/local
			AC_MSG_CHECKING([python libdir])
			if test -z "`echo $PYTHON_LIBDIR | grep "/usr/lib"`" ; then
				PYTHON_LDFLAGS="-L$PYTHON_LIBDIR $PYTHON_LDFLAGS"
				LIBS="-L$PYTHON_LIBDIR $LIBS"
			fi
			AC_MSG_RESULT([$PYTHON_LIBDIR])

			# check libpython
			AC_CHECK_LIB([$PYTHON_LIB], [main], [has_libpython="yes"], [has_libpython="no"])

			if test "$has_libpython" = "no" ; then
				AS_IF([test "$with_python" = "try"],
				 	[AC_MSG_WARN([$PYTHON_LIB is unusable])],
					[AC_MSG_ERROR([$PYTHON_LIB is unusable])]
				)
			fi

			# check whether system libpython is usable and has threads support
			CFLAGS="$PYTHON_CFLAGS"
			LIBS="$PYTHON_LDFLAGS"
			AC_CHECK_FUNC([PyThread_init_thread], [python_has_threads="yes"], [python_has_threads="no"])

			if test "$python_has_threads" = "no"; then
				AS_IF([test "$with_python" = "try"],
					[AC_MSG_WARN([Your python lacks threads support, can not build mod_python])],
					[AC_MSG_ERROR([Your python lacks threads support, can not build mod_python])]
				)
			else
				AC_MSG_NOTICE([Your python seems OK, do not forget to enable mod_python in modules.conf])
				AC_SUBST([PYTHON_CFLAGS],  [$PYTHON_CFLAGS])
				AC_SUBST([PYTHON_LDFLAGS], [$PYTHON_LDFLAGS])
			fi
		else
			AS_IF([test "$with_python" = "try"],
				[AC_MSG_WARN([Could not find or use python distutils module: $python_result])],
				[AC_MSG_ERROR([Could not find or use python distutils module: $python_result])]
			)
		fi

		LIBS="$save_LIBS"
		CFLAGS="$save_CFLAGS"

		unset python_has_threads
		unset python_has_distutils
		unset python_result
	else
		AS_IF([test "$with_python" = "try"],
			[AC_MSG_WARN([Could not find python, mod_python will not build, use --with-python to specify the location])],
			[AC_MSG_ERROR([Could not find python, use --with-python to specify the location])]
		)
	fi
else
	AC_MSG_WARN([python support disabled, building mod_python will fail!])
fi

CHECK_ERLANG

# we never use this, and hard setting it will make cross compile work better
ac_cv_file_dbd_apr_dbd_mysql_c=no

AC_CONFIG_FILES([Makefile
		build/Makefile
		src/Makefile
		src/mod/Makefile
		src/mod/applications/mod_abstraction/Makefile
		src/mod/applications/mod_avmd/Makefile
		src/mod/applications/mod_bert/Makefile
		src/mod/applications/mod_blacklist/Makefile
		src/mod/applications/mod_callcenter/Makefile
		src/mod/applications/mod_cidlookup/Makefile
		src/mod/applications/mod_cluechoo/Makefile
		src/mod/applications/mod_commands/Makefile
		src/mod/applications/mod_conference/Makefile
		src/mod/applications/mod_curl/Makefile
		src/mod/applications/mod_db/Makefile
		src/mod/applications/mod_directory/Makefile
		src/mod/applications/mod_distributor/Makefile
		src/mod/applications/mod_dptools/Makefile
		src/mod/applications/mod_easyroute/Makefile
		src/mod/applications/mod_enum/Makefile
		src/mod/applications/mod_esf/Makefile
		src/mod/applications/mod_esl/Makefile
		src/mod/applications/mod_expr/Makefile
		src/mod/applications/mod_fifo/Makefile
		src/mod/applications/mod_fsk/Makefile
		src/mod/applications/mod_fsv/Makefile
		src/mod/applications/mod_hash/Makefile
		src/mod/applications/mod_httapi/Makefile
		src/mod/applications/mod_http_cache/Makefile
		src/mod/applications/mod_ladspa/Makefile
		src/mod/applications/mod_lcr/Makefile
		src/mod/applications/mod_limit/Makefile
		src/mod/applications/mod_memcache/Makefile
		src/mod/applications/mod_mongo/Makefile
		src/mod/applications/mod_mp4/Makefile
		src/mod/applications/mod_nibblebill/Makefile
		src/mod/applications/mod_oreka/Makefile
		src/mod/applications/mod_osp/Makefile
		src/mod/applications/mod_prefix/Makefile
		src/mod/applications/mod_rad_auth/Makefile
		src/mod/applications/mod_random/Makefile
		src/mod/applications/mod_redis/Makefile
		src/mod/applications/mod_rss/Makefile
		src/mod/applications/mod_skel/Makefile
		src/mod/applications/mod_sms/Makefile
		src/mod/applications/mod_snapshot/Makefile
		src/mod/applications/mod_snipe_hunt/Makefile
		src/mod/applications/mod_snom/Makefile
		src/mod/applications/mod_sonar/Makefile
		src/mod/applications/mod_soundtouch/Makefile
		src/mod/applications/mod_spandsp/Makefile
		src/mod/applications/mod_spy/Makefile
		src/mod/applications/mod_stress/Makefile
		src/mod/applications/mod_translate/Makefile
		src/mod/applications/mod_valet_parking/Makefile
		src/mod/applications/mod_vmd/Makefile
		src/mod/applications/mod_voicemail/Makefile
		src/mod/applications/mod_voicemail_ivr/Makefile
		src/mod/asr_tts/mod_cepstral/Makefile
		src/mod/asr_tts/mod_flite/Makefile
		src/mod/asr_tts/mod_pocketsphinx/Makefile
		src/mod/asr_tts/mod_tts_commandline/Makefile
		src/mod/asr_tts/mod_unimrcp/Makefile
		src/mod/codecs/mod_amr/Makefile
		src/mod/codecs/mod_amrwb/Makefile
		src/mod/codecs/mod_b64/Makefile
		src/mod/codecs/mod_bv/Makefile
		src/mod/codecs/mod_celt/Makefile
		src/mod/codecs/mod_codec2/Makefile
		src/mod/codecs/mod_com_g729/Makefile
		src/mod/codecs/mod_dahdi_codec/Makefile
		src/mod/codecs/mod_g723_1/Makefile
		src/mod/codecs/mod_g729/Makefile
		src/mod/codecs/mod_h26x/Makefile
		src/mod/codecs/mod_ilbc/Makefile
		src/mod/codecs/mod_isac/Makefile
		src/mod/codecs/mod_mp4v/Makefile
		src/mod/codecs/mod_opus/Makefile
		src/mod/codecs/mod_sangoma_codec/Makefile
		src/mod/codecs/mod_silk/Makefile
		src/mod/codecs/mod_siren/Makefile
		src/mod/codecs/mod_skel_codec/Makefile
		src/mod/codecs/mod_theora/Makefile
		src/mod/codecs/mod_vp8/Makefile
		src/mod/dialplans/mod_dialplan_asterisk/Makefile
		src/mod/dialplans/mod_dialplan_directory/Makefile
		src/mod/dialplans/mod_dialplan_xml/Makefile
		src/mod/directories/mod_ldap/Makefile
		src/mod/endpoints/mod_alsa/Makefile
		src/mod/endpoints/mod_dingaling/Makefile
		src/mod/endpoints/mod_gsmopen/Makefile
		src/mod/endpoints/mod_h323/Makefile
		src/mod/endpoints/mod_khomp/Makefile
		src/mod/endpoints/mod_loopback/Makefile
		src/mod/endpoints/mod_opal/Makefile
		src/mod/endpoints/mod_portaudio/Makefile
		src/mod/endpoints/mod_reference/Makefile
		src/mod/endpoints/mod_rtmp/Makefile
		src/mod/endpoints/mod_skinny/Makefile
		src/mod/endpoints/mod_skypopen/Makefile
		src/mod/endpoints/mod_sofia/Makefile
		src/mod/endpoints/mod_unicall/Makefile
		src/mod/endpoints/mod_rtc/Makefile
		src/mod/endpoints/mod_verto/Makefile
		src/mod/event_handlers/mod_amqp/Makefile
		src/mod/event_handlers/mod_cdr_csv/Makefile
		src/mod/event_handlers/mod_cdr_mongodb/Makefile
		src/mod/event_handlers/mod_cdr_pg_csv/Makefile
		src/mod/event_handlers/mod_cdr_sqlite/Makefile
		src/mod/event_handlers/mod_erlang_event/Makefile
		src/mod/event_handlers/mod_event_multicast/Makefile
		src/mod/event_handlers/mod_event_socket/Makefile
		src/mod/event_handlers/mod_event_test/Makefile
		src/mod/event_handlers/mod_format_cdr/Makefile
		src/mod/event_handlers/mod_json_cdr/Makefile
		src/mod/event_handlers/mod_radius_cdr/Makefile
		src/mod/event_handlers/mod_odbc_cdr/Makefile
		src/mod/event_handlers/mod_rayo/Makefile
		src/mod/event_handlers/mod_snmp/Makefile
		src/mod/event_handlers/mod_event_zmq/Makefile
		src/mod/formats/mod_local_stream/Makefile
		src/mod/formats/mod_native_file/Makefile
		src/mod/formats/mod_shell_stream/Makefile
		src/mod/formats/mod_shout/Makefile
		src/mod/formats/mod_sndfile/Makefile
		src/mod/formats/mod_ssml/Makefile
		src/mod/formats/mod_tone_stream/Makefile
		src/mod/formats/mod_vlc/Makefile
		src/mod/formats/mod_portaudio_stream/Makefile
		src/mod/languages/mod_java/Makefile
		src/mod/languages/mod_lua/Makefile
		src/mod/languages/mod_managed/Makefile
		src/mod/languages/mod_perl/Makefile
		src/mod/languages/mod_python/Makefile
		src/mod/languages/mod_v8/Makefile
		src/mod/languages/mod_yaml/Makefile
		src/mod/languages/mod_basic/Makefile
		src/mod/legacy/languages/mod_lua/Makefile
		src/mod/loggers/mod_console/Makefile
		src/mod/loggers/mod_graylog2/Makefile
		src/mod/loggers/mod_logfile/Makefile
		src/mod/loggers/mod_syslog/Makefile
		src/mod/say/mod_say_de/Makefile
		src/mod/say/mod_say_en/Makefile
		src/mod/say/mod_say_es/Makefile
		src/mod/say/mod_say_es_ar/Makefile
		src/mod/say/mod_say_fa/Makefile
		src/mod/say/mod_say_fr/Makefile
		src/mod/say/mod_say_he/Makefile
		src/mod/say/mod_say_hr/Makefile
		src/mod/say/mod_say_hu/Makefile
		src/mod/say/mod_say_it/Makefile
		src/mod/say/mod_say_ja/Makefile
		src/mod/say/mod_say_nl/Makefile
		src/mod/say/mod_say_pl/Makefile
		src/mod/say/mod_say_pt/Makefile
		src/mod/say/mod_say_ru/Makefile
		src/mod/say/mod_say_sv/Makefile
		src/mod/say/mod_say_th/Makefile
		src/mod/say/mod_say_zh/Makefile
		src/mod/timers/mod_posix_timer/Makefile
		src/mod/timers/mod_timerfd/Makefile
		src/mod/xml_int/mod_xml_cdr/Makefile
		src/mod/xml_int/mod_xml_curl/Makefile
		src/mod/xml_int/mod_xml_ldap/Makefile
		src/mod/xml_int/mod_xml_radius/Makefile
		src/mod/xml_int/mod_xml_rpc/Makefile
		src/mod/xml_int/mod_xml_scgi/Makefile
		src/include/switch_am_config.h
		build/getsounds.sh
		build/getlib.sh
		build/getg729.sh
		build/freeswitch.pc
		build/modmake.rules
                libs/esl/Makefile
                libs/esl/perl/Makefile
                libs/esl/php/Makefile
		libs/xmlrpc-c/include/xmlrpc-c/config.h
		libs/xmlrpc-c/xmlrpc_config.h
		libs/xmlrpc-c/config.mk
		libs/xmlrpc-c/srcdir.mk
		libs/xmlrpc-c/stamp-h
		scripts/gentls_cert])

AM_CONDITIONAL(ISLINUX, [test `uname -s` = Linux])
AM_CONDITIONAL(ISMAC, [test `uname -s` = Darwin])
AM_CONDITIONAL(ISFREEBSD, [test `uname -s` = FreeBSD])
AM_CONDITIONAL(IS64BITLINUX, [test `uname -m` = x86_64])

AM_CONDITIONAL(HAVE_AMR, [ test -d ${switch_srcdir}/libs/amr ])
AM_CONDITIONAL(HAVE_AMRWB, [ test -d ${switch_srcdir}/libs/amrwb ])
AM_CONDITIONAL(HAVE_G723_1, [ test -d ${switch_srcdir}/libs/libg723_1 ])
AM_CONDITIONAL(HAVE_G729, [ test -d ${switch_srcdir}/libs/libg729 ])

#some vars to sub into the Makefile.am's
#LIBS+=> core.log || error="yes";if test -n "$(VERBOSE)" -o "$$error" = "yes";then cat core.log;fi;if test "$$error" = "yes";then exit 1;fi
LIBTOOL='$(SHELL) $(switch_builddir)/libtool'
TOUCH_TARGET='if test -f "$@";then touch "$@";fi;'
CONF_MODULES='$$(grep -v "\#" $(switch_builddir)/modules.conf | sed -e "s|^.*/||" | sort | uniq )'
CONF_DISABLED_MODULES='$$(grep "\#" $(switch_builddir)/modules.conf | grep -v "\#\#" | sed -e "s|^.*/||" | sort | uniq )'
OUR_MODS='$$(if test -z "$(MODULES)" ; then tmp_mods="$(CONF_MODULES)"; else tmp_mods="$(MODULES)" ; fi ; mods="$$(for i in $$tmp_mods ; do echo $$i-all ; done )"; echo $$mods )'
OUR_CLEAN_MODS='$$(if test -z "$(MODULES)" ; then tmp_mods="$(CONF_MODULES)"; else tmp_mods="$(MODULES)" ; fi ; mods="$$(for i in $$tmp_mods ; do echo $$i-clean ; done )"; echo $$mods )'
OUR_INSTALL_MODS='$$(if test -z "$(MODULES)" ; then tmp_mods="$(CONF_MODULES)"; else tmp_mods="$(MODULES)" ; fi ; mods="$$(for i in $$tmp_mods ; do echo $$i-install ; done)"; echo $$mods )'
OUR_UNINSTALL_MODS='$$(if test -z "$(MODULES)" ; then tmp_mods="$(CONF_MODULES)"; else tmp_mods="$(MODULES)" ; fi ; mods="$$(for i in $$tmp_mods ; do echo $$i-uninstall ; done)"; echo $$mods )'
OUR_DISABLED_MODS='$$(tmp_mods="$(CONF_DISABLED_MODULES)"; mods="$$(for i in $$tmp_mods ; do echo $$i-all ; done )"; echo $$mods )'
OUR_DISABLED_CLEAN_MODS='$$(tmp_mods="$(CONF_DISABLED_MODULES)";  mods="$$(for i in $$tmp_mods ; do echo $$i-clean ; done )"; echo $$mods )'
OUR_DISABLED_INSTALL_MODS='$$(tmp_mods="$(CONF_DISABLED_MODULES)"; mods="$$(for i in $$tmp_mods ; do echo $$i-install ; done)"; echo $$mods )'
OUR_DISABLED_UNINSTALL_MODS='$$(tmp_mods="$(CONF_DISABLED_MODULES)"; mods="$$(for i in $$tmp_mods ; do echo $$i-uninstall ; done)"; echo $$mods )'

#AM_MAKEFLAGS='"OUR_MODULES=$(OUR_MODS)" "OUR_CLEAN_MODULES=$(OUR_CLEAN_MODS)" "OUR_INSTALL_MODULES=$(OUR_INSTALL_MODS)" "OUR_UNINSTALL_MODULES=$(OUR_UNINSTALL_MODS)" "OUR_DISABLED_MODULES=$(OUR_DISABLED_MODS)" "OUR_DISABLED_CLEAN_MODULES=$(OUR_DISABLED_CLEAN_MODS)" "OUR_DISABLED_INSTALL_MODULES=$(OUR_DISABLED_INSTALL_MODS)" "OUR_DISABLED_UNINSTALL_MODULES=$(OUR_DISABLED_UNINSTALL_MODS)" `test -n "$(VERBOSE)" || echo -s`'
#AM_MAKEFLAGS='`test -n "$(VERBOSE)" || echo -s`'
AC_SUBST(LIBTOOL)
AC_SUBST(TOUCH_TARGET)
AC_SUBST(CONF_DISABLED_MODULES)
AC_SUBST(CONF_MODULES)

AC_SUBST(OUR_MODS)
AC_SUBST(OUR_CLEAN_MODS)
AC_SUBST(OUR_INSTALL_MODS)
AC_SUBST(OUR_UNINSTALL_MODS)
AC_SUBST(OUR_DISABLED_MODS)
AC_SUBST(OUR_DISABLED_CLEAN_MODS)
AC_SUBST(OUR_DISABLED_INSTALL_MODS)
AC_SUBST(OUR_DISABLED_UNINSTALL_MODS)
AC_SUBST(AM_MAKEFLAGS)

ac_configure_args="$ac_configure_args CONFIGURE_CFLAGS='$CFLAGS $CPPFLAGS' CONFIGURE_CXXFLAGS='$CXXFLAGS $CPPFLAGS' CONFIGURE_LDFLAGS='$LDFLAGS' "

#	--prefix='$prefix' --exec_prefix='$exec_prefix' --libdir='$libdir' --libexecdir='$libexecdir' --bindir='$bindir' --sbindir='$sbindir' \
#	--localstatedir='$localstatedir' --datadir='$datadir'"

# Run configure in all the subdirs
AC_CONFIG_SUBDIRS([libs/srtp])
if test "$use_system_apr" != "yes"; then
   AC_CONFIG_SUBDIRS([libs/apr])
fi
if test "$use_system_aprutil" != "yes"; then
   AC_CONFIG_SUBDIRS([libs/apr-util])
fi
AC_CONFIG_SUBDIRS([libs/ilbc])
AC_CONFIG_SUBDIRS([libs/iksemel])
AC_CONFIG_SUBDIRS([libs/libdingaling])
AC_CONFIG_SUBDIRS([libs/libsndfile])
AC_CONFIG_SUBDIRS([libs/sofia-sip])
AC_CONFIG_SUBDIRS([libs/freetdm])
AC_CONFIG_SUBDIRS([libs/unimrcp])
AC_CONFIG_SUBDIRS([libs/tiff-4.0.2])
AC_CONFIG_SUBDIRS([libs/spandsp])
AC_CONFIG_SUBDIRS([libs/broadvoice])
AC_CONFIG_SUBDIRS([libs/libg722_1])
AC_CONFIG_SUBDIRS([libs/silk])
AC_CONFIG_SUBDIRS([libs/libcodec2])
if test "x${enable_zrtp}" = "xyes"; then
   AC_CONFIG_SUBDIRS([libs/libzrtp])
fi

case $host in
   *-openbsd*|*-netbsd*)
       # libtool won't link static libs against shared ones on NetBSD/OpenBSD unless we tell it not to be stupid
       AC_CONFIG_COMMANDS([hacklibtool], [cp libtool libtool.orig  && sed -e "s/deplibs_check_method=.*/deplibs_check_method=pass_all/g" libtool.orig > libtool])
       ;;
esac


AC_OUTPUT

##
## Registering for ClueCon
##
if ! test -f noreg ; then
echo ""
echo ""
echo $ECHO_N "Registering you for ClueCon http://www.cluecon.com $ECHO_C" 1>&6
sleep 1
echo $ECHO_N ".$ECHO_C" 1>&6
sleep 1
echo $ECHO_N ".$ECHO_C" 1>&6
sleep 1
AC_MSG_RESULT([ See you in August. ;-)])
echo ""
fi

##
## Configuration summary
##

echo
echo "-------------------------- FreeSWITCH configuration --------------------------"
echo ""
echo "  Locations:"
echo "      FHS enabled:     ${enable_fhs}"
echo ""
echo "      prefix:          ${prefix}"
echo "      exec_prefix:     ${exec_prefix}"
echo "      bindir:          ${bindir}"
echo "      sysconfdir:      ${sysconfdir}"
echo "      libdir:          ${libdir}"
echo ""
echo "      certsdir:        ${certsdir}"
echo "      dbdir:           ${dbdir}"
echo "      grammardir:      ${grammardir}"
echo "      htdocsdir:       ${htdocsdir}"
echo "      logfiledir:      ${logfiledir}"
echo "      modulesdir:      ${modulesdir}"
echo "      pkgconfigdir:    ${pkgconfigdir}"
echo "      recordingsdir:   ${recordingsdir}"
echo "      runtimedir:      ${runtimedir}"
echo "      scriptdir:       ${scriptdir}"
echo "      soundsdir:       ${soundsdir}"
echo "      storagedir:      ${storagedir}"
echo "      cachedir:        ${cachedir}"
echo ""
echo "------------------------------------------------------------------------------"

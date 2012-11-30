dnl Process this file with autoconf to produce a configure script.

AC_INIT(include/xmlrpc-c/base.h)
AM_CONFIG_HEADER(xmlrpc_amconfig.h)

dnl =======================================================================
dnl Define PACKAGE, VERSION, @PACKAGE@, @VERSION@
dnl =======================================================================

dnl "x.xx" is supposed to be a version number, but is meaningless here.
dnl The real version number is in Makefile.version.
AM_INIT_AUTOMAKE(xmlrpc-c, x.xx)

dnl Define @build@, @build_cpu@, @build_vendor@, @build_os,
dnl @host, @host_cpu@, @host_vender, and @host_os@ substitutions.
dnl "host" means the target system -- the one for which we are building.
dnl "build" means the system that will do the building.
AC_CANONICAL_HOST

dnl =======================================================================
dnl Decide What To Build
dnl =======================================================================

FEATURE_LIST=

AC_ARG_ENABLE(wininet-client,
  [  --disable-wininet-client  Don't build the Wininet client transport], , 
enable_wininet_client=maybe)

if test $enable_wininet_client = maybe; then
  AC_CHECK_PROG(have_wininet_config, wininet-config, yes, no)
  if test $have_wininet_config = no; then
    AC_MSG_NOTICE([You don't appear to have Wininet installed (no working wininet-config in your command search path), so we will not build the Wininet client XML transport])
    MUST_BUILD_WININET_CLIENT=no
  else
    MUST_BUILD_WININET_CLIENT=yes
  fi
else
  MUST_BUILD_WININET_CLIENT=$enable_wininet_client
fi

AC_MSG_CHECKING(whether to build Wininet client XML transport module)
AC_MSG_RESULT($MUST_BUILD_WININET_CLIENT)
AC_SUBST(MUST_BUILD_WININET_CLIENT)


AC_ARG_ENABLE(curl-client,
  [  --disable-curl-client     Don't build the Curl client transport], , 
enable_curl_client=maybe)

if test $enable_curl_client = maybe; then
  AC_CHECK_PROG(have_curl_config, curl-config, yes, no)
  if test $have_curl_config = no; then
    AC_MSG_NOTICE([You don't appear to have Curl installed (no working curl-config in your command search path), so we will not build the Curl client XML transport])
    MUST_BUILD_CURL_CLIENT=no
  else
    MUST_BUILD_CURL_CLIENT=yes
  fi
else
  MUST_BUILD_CURL_CLIENT=$enable_curl_client
fi

AC_MSG_CHECKING(whether to build Curl client XML transport module)
AC_MSG_RESULT($MUST_BUILD_CURL_CLIENT)
AC_SUBST(MUST_BUILD_CURL_CLIENT)


AC_ARG_ENABLE(libwww-client,
  [  --disable-libwww-client   Don't build the Libwww client transport], , 
enable_libwww_client=maybe)

if test $enable_libwww_client = maybe; then
  AC_CHECK_PROG(have_libwww_config, libwww-config, yes, no)
  if test $have_libwww_config = no; then
    AC_MSG_NOTICE([You don't appear to have Libwww installed (no working libwww-config in your command search path), so we will not build the Libwww client XML transport])
    MUST_BUILD_LIBWWW_CLIENT=no
  else
    MUST_BUILD_LIBWWW_CLIENT=yes
  fi
else
  MUST_BUILD_LIBWWW_CLIENT=$enable_libwww_client
fi

AC_MSG_CHECKING(whether to build Libwww client XML transport module)
AC_MSG_RESULT($MUST_BUILD_LIBWWW_CLIENT)
AC_SUBST(MUST_BUILD_LIBWWW_CLIENT)


AC_ARG_ENABLE(tools,
  [  --enable-tools            Build the tools], , 
[enable_tools=no])

# The first AC_CHECK_LIB has to be in unconditional code because as a
# side effect, it determines what the object file suffix is on this system,
# and if it is statically present even though not actually executed, Autoconf
# later thinks it has already computed the object file suffix and uses it
# without computing it.  This was with Autoconf 2.59
AC_CHECK_LIB(ncurses, main, [have_libncurses=yes], [have_libncurses=no])
AC_CHECK_LIB(readline, main, [have_libreadline=yes], [have_libreadline=no])

AC_MSG_CHECKING(whether to build tools)

BUILD_XMLRPC_PSTREAM=no

if test $enable_tools = yes; then
  if ! test "$MUST_BUILD_WININET_CLIENT $MUST_BUILD_CURL_CLIENT $MUST_BUILD_LIBWWW_CLIENT" = "no no no"; then
    if test $have_libreadline = yes && test $have_libncurses = yes; then
      BUILD_XMLRPC_PSTREAM=yes
    fi
    BUILD_TOOLS=yes
  fi
else
  BUILD_TOOLS=$enable_tools
fi

AC_MSG_RESULT($BUILD_TOOLS)
AC_SUBST(BUILD_TOOLS)

if test $BUILD_TOOLS = yes; then
  AC_MSG_CHECKING(whether to build the xmlrpc_pstream tool)
  AC_MSG_RESULT($BUILD_XMLRPC_PSTREAM)
  AC_SUBST(BUILD_XMLRPC_PSTREAM)
fi


dnl Set up the appropriate Makefile substitutions.

CLIENTTEST=clienttest
AC_SUBST(CLIENTTEST)
XMLRPC_CLIENT_H=xmlrpc_client.h
AC_SUBST(XMLRPC_CLIENT_H)
XMLRPC_TRANSPORT_H=xmlrpc_transport.h
AC_SUBST(XMLRPC_TRANSPORT_H)
SYNCH_CLIENT=synch_client
AC_SUBST(SYNCH_CLIENT)
ASYNCH_CLIENT=asynch_client
AC_SUBST(ASYNCH_CLIENT)
AUTH_CLIENT=auth_client
AC_SUBST(AUTH_CLIENT)
QUERY_MEERKAT=query-meerkat
AC_SUBST(QUERY_MEERKAT)

if test $MUST_BUILD_WININET_CLIENT = yes; then
    FEATURE_LIST="wininet-client $FEATURE_LIST"
fi
if test $MUST_BUILD_CURL_CLIENT = yes; then
    FEATURE_LIST="curl-client $FEATURE_LIST"
fi
if test $MUST_BUILD_LIBWWW_CLIENT = yes; then
    FEATURE_LIST="libwww-client $FEATURE_LIST"
fi

dnl Check to see if we should build our Abyss server module.
AC_MSG_CHECKING(whether to build Abyss server module)
AC_ARG_ENABLE(abyss-server,
  [  --disable-abyss-server    Don't build the Abyss server module], ,
enable_abyss_server=yes)
AC_MSG_RESULT($enable_abyss_server)
ENABLE_ABYSS_SERVER=$enable_abyss_server
AC_SUBST(ENABLE_ABYSS_SERVER)

dnl Set up the appropriate Makefile substitutions.
ABYSS_SUBDIR=
SERVERTEST=
VALIDATEE=
XMLRPC_ABYSS_H=
SERVER=
if test x"$enable_abyss_server" != xno; then
    FEATURE_LIST="abyss-server $FEATURE_LIST"
    ABYSS_SUBDIR=abyss
    SERVERTEST=servertest
    VALIDATEE=validatee
    XMLRPC_ABYSS_H=xmlrpc_abyss.h
    SERVER=server
fi
AC_SUBST(ABYSS_SUBDIR)
AC_SUBST(SERVERTEST)
AC_SUBST(VALIDATEE)
AC_SUBST(XMLRPC_ABYSS_H)
AC_SUBST(SERVER)

dnl Check to see if we should build our CGI server module.
AC_MSG_CHECKING(whether to build CGI server module)
AC_ARG_ENABLE(cgi-server,
  [  --disable-cgi-server      Don't build the CGI server module], ,
enable_cgi_server=yes)
AC_MSG_RESULT($enable_cgi_server)
ENABLE_CGI_SERVER=$enable_cgi_server
AC_SUBST(ENABLE_CGI_SERVER)

dnl Check to see if we should build our C++ stuff.
AC_MSG_CHECKING(whether to build C++ wrappers and tools)
AC_ARG_ENABLE(cplusplus,
  [  --disable-cplusplus       Don't build the C++ wrapper classes or tools], ,
enable_cplusplus=yes)
AC_MSG_RESULT($enable_cplusplus)
ENABLE_CPLUSPLUS=$enable_cplusplus
AC_SUBST(ENABLE_CPLUSPLUS)

dnl Set up the appropriate Makefile substitutions.
LIBXMLRPC_CPP_A=
CPPTEST=
XMLRPCCPP_H=
XML_RPC_API2CPP_SUBDIR=
MEERKAT_APP_LIST=
INTEROP_CLIENT_SUBDIR=
if test x"$enable_cplusplus" != xno; then
    FEATURE_LIST="c++ $FEATURE_LIST"
    LIBXMLRPC_CPP_A=libxmlrpc_cpp.a
    CPPTEST=cpptest
    XMLRPCCPP_H=XmlRpcCpp.h

    if test $MUST_BUILD_LIBWWW_CLIENT = yes; then
        XML_RPC_API2CPP_SUBDIR=xml-rpc-api2cpp
    elif test $MUST_BUILD_CURL_CLIENT = yes; then
        XML_RPC_API2CPP_SUBDIR=xml-rpc-api2cpp
    fi
fi
AC_SUBST(LIBXMLRPC_CPP_A)
AC_SUBST(CPPTEST)
AC_SUBST(XMLRPCCPP_H)
AC_SUBST(XML_RPC_API2CPP_SUBDIR)


AC_SUBST(FEATURE_LIST)


dnl =======================================================================
dnl Checks for programs.
dnl =======================================================================

AC_PROG_CC
if test x"$enable_cplusplus" != xno; then
    AC_PROG_CXX
fi


dnl =======================================================================
dnl Checks for libraries.
dnl =======================================================================

# Code by albert chin <china@thewrittenword.com> to check for various
# oddball networking libraries.  Solaris and some other operating systems
# hide their networking code in various places.  (Yes, this links too many
# of our libraries against -lsocket, but a finer-grained mechanism would
# require too much testing.)
AC_CHECK_FUNC(socket, , AC_CHECK_LIB(socket, socket))

# Above sets LIBS, which is not all that useful because we don't want
# to include every library in every link.  It also sets
# ac_cv_lib_socket_socket, which we use to pass more specific information
# to the configuration files.

if test x"$ac_cv_lib_socket_socket" = xyes; then
  LSOCKET=-lsocket
else
  LSOCKET=
fi
AC_SUBST(LSOCKET)

# For some reason, we don't seem to need this on Solaris.  If you do
# need it, go ahead and try it.
# AC_CHECK_FUNC(gethostent, , AC_CHECK_LIB(nsl, gethostent))


dnl =======================================================================
dnl Checks for header files.
dnl =======================================================================

AC_STDC_HEADERS

dnl We don't use AM_CONFIG_HEADER to define HAVE_WCHAR_H, etc. because
dnl the following is more straightforward and easier to understand,
dnl especially for a newcomer.  Furthermore, AM_CONFIG_HEADER represents
dnl false as undefined, whereas our scheme represents it as 0.  undefined
dnl is a poor choice because it often means just that you neglected to
dnl choose a value for some reason.

dnl defines ac_cv_header_wchar_h, etc:
AC_CHECK_HEADERS(wchar.h)

if test x"$ac_cv_header_wchar_h" = xyes; then
  HAVE_WCHAR_H_DEFINE=1
else
  HAVE_WCHAR_H_DEFINE=0
fi
AC_SUBST(HAVE_WCHAR_H_DEFINE)

# Needed by Abyss on Solaris:

AC_CHECK_HEADERS(sys/filio.h)
if test x"$ac_cv_header_sys_filio_h" = xyes; then
  HAVE_SYS_FILIO_H_DEFINE=1
else
  HAVE_SYS_FILIO_H_DEFINE=0
fi
AC_SUBST(HAVE_SYS_FILIO_H_DEFINE)

# Needed by Abyss on Solaris:

AC_CHECK_HEADERS(sys/ioctl.h)
if test x"$ac_cv_header_sys_ioctl_h" = xyes; then
  HAVE_SYS_IOCTL_H_DEFINE=1
else
  HAVE_SYS_IOCTL_H_DEFINE=0
fi
AC_SUBST(HAVE_SYS_IOCTL_H_DEFINE)

AC_CHECK_HEADERS(sys/select.h)
if test x"$ac_cv_header_sys_select_h" = xyes; then
  HAVE_SYS_SELECT_H_DEFINE=1
else
  HAVE_SYS_SELECT_H_DEFINE=0
fi
AC_SUBST(HAVE_SYS_SELECT_H_DEFINE)


AC_CHECK_HEADERS(stdarg.h, , [
AC_MSG_ERROR(stdarg.h is required to build this library)
])


dnl =======================================================================
dnl Checks for typedefs, structures, and compiler characteristics.
dnl =======================================================================

dnl AC_C_BIGENDIAN
AC_TYPE_SIZE_T

dnl This check is borrowed from Python 1.5.2.
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


dnl =======================================================================
dnl Checks for library functions.
dnl =======================================================================

AC_CHECK_FUNC(vsnprintf, , [
AC_MSG_ERROR(your C library does not provide vsnprintf)
])

dnl Unicode function needed by test suites.
AC_CHECK_FUNCS(wcsncmp)

dnl CygWin doesn't provide setgroups.
AC_CHECK_FUNCS(setgroups)

AC_CHECK_FUNCS(asprintf)

AC_CHECK_FUNCS(setenv strtoll strtoull strtoq strtouq __strtoll __strtoull)

dnl uclib doesn't have pselect
AC_CHECK_FUNCS(pselect)

dnl Windows doesn't have gettimeofday, localtime_r, or gmtime_r
AC_CHECK_FUNCS(gettimeofday)
AC_CHECK_FUNCS(localtime_r)
AC_CHECK_FUNCS(gmtime_r)

dnl Windows doesn't have strcasecmp;
AC_CHECK_FUNCS(strcasecmp)
AC_CHECK_FUNCS(stricmp)
AC_CHECK_FUNCS(_stricmp)

dnl =======================================================================
dnl Checks for operating system features.
dnl =======================================================================

dnl Non-Unix systems will need to set up their platform configuration file
dnl by hand.
DIRECTORY_SEPARATOR="/"
AC_SUBST(DIRECTORY_SEPARATOR)


dnl =======================================================================
dnl ABYSS Configuration
dnl =======================================================================

AC_MSG_CHECKING(whether to use Abyss pthread function)
AC_ARG_ENABLE(abyss-threads,
  [  --disable-abyss-threads   Use fork in Abyss instead of pthreads], ,
  enable_abyss_threads=yes)
AC_MSG_RESULT($enable_abyss_threads)

ENABLE_ABYSS_THREADS=$enable_abyss_threads
AC_SUBST(ENABLE_ABYSS_THREADS)

if test x"$enable_abyss_threads" != xno; then
    CFLAGS="$CFLAGS -D_THREAD"
fi


dnl =======================================================================
dnl Finding wininet stubs
dnl =======================================================================
dnl If you implement the parts of wininet.h the wininet_transport uses,
dnl you will need to configure this way..

if test $MUST_BUILD_WININET_CLIENT = yes; then

    dnl You can control which of these gets chosen by controlling PATH.
    AC_PATH_PROGS(WININET_CONFIG, wininet-xmlrpc-config wininet-config, no)
    if test "x$WININET_CONFIG" = "xno"; then
        AC_MSG_ERROR(Configure INTERNAL ERROR - first wininet-config found, then not found)
    fi

    dnl Get our wininet version.
    dnl Adapted from a macro which called gtk-config.
    AC_MSG_CHECKING(for wininet version >= 1.0.0)
    W3VER=$($WININET_CONFIG --version)
    WININET_MAJOR=\
$(echo $W3VER|sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\1/')
    WININET_MINOR=\
$(echo $W3VER|sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\2/')
    WININET_MICRO=\
$(echo $W3VER|sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\3/')
    AC_MSG_RESULT($WININET_MAJOR.$WININET_MINOR.$WININET_MICRO)

    dnl Check to make sure our version is OK.
    WININET_VERSION_OK=yes
    if test $WININET_MAJOR -lt 1; then
        WININET_VERSION_OK=no
    else
        if test $WININET_MAJOR -eq 1 -a $WININET_MINOR -lt 0; then
            WININET_VERSION_OK=no
        else
            if test $WININET_MAJOR -eq 1 -a $WININET_MINOR -eq 0 \
                -a $WININET_MICRO -lt 0; then
                WININET_VERSION_OK=no
            fi
        fi
    fi
    if test "x$WININET_VERSION_OK" = "xno"; then
        AC_MSG_ERROR(wininet version >= 1.0.0 required)
    fi

    dnl Get the necessary CFLAGS, and merge them into our master list.
    WININET_CFLAGS=$($WININET_CONFIG --cflags)
    AC_SUBST(WININET_CFLAGS)
    CFLAGS="$CFLAGS $WININET_CFLAGS" 

    dnl Get the huge list of libraries we need to link against.
    WININET_LDADD=$($WININET_CONFIG --libs)
    AC_SUBST(WININET_LDADD)

    AC_MSG_CHECKING(for wininet library directory)
    WININET_LIBDIR="$($WININET_CONFIG --prefix)/lib"
    AC_MSG_RESULT($WININET_LIBDIR)
    AC_SUBST(WININET_LIBDIR)

fi # MUST_BUILD_WININET_CLIENT

dnl =======================================================================
dnl Finding w3c-libwww
dnl =======================================================================

if test $MUST_BUILD_LIBWWW_CLIENT = yes; then

    dnl First of all, locate the libwww config program.
    dnl You can control which of these gets chosen by controlling PATH.
    AC_PATH_PROGS(LIBWWW_CONFIG, libwww-xmlrpc-config libwww-config, no)
    if test "x$LIBWWW_CONFIG" = "xno"; then
        AC_MSG_ERROR(Configure INTERNAL ERROR - first libwww-config found, then not found)
    fi

    dnl Get our libwww version.
    dnl Adapted from a macro which called gtk-config.
    AC_MSG_CHECKING(for w3c-libwww version >= 5.2.8)
    W3VER=$($LIBWWW_CONFIG --version)
    LIBWWW_MAJOR=\
$(echo $W3VER|sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\1/')
    LIBWWW_MINOR=\
$(echo $W3VER|sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\2/')
    LIBWWW_MICRO=\
$(echo $W3VER|sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\3/')
    AC_MSG_RESULT($LIBWWW_MAJOR.$LIBWWW_MINOR.$LIBWWW_MICRO)

    dnl Check to make sure our version is OK.
    LIBWWW_VERSION_OK=yes
    if test $LIBWWW_MAJOR -lt 5; then
        LIBWWW_VERSION_OK=no
    else
        if test $LIBWWW_MAJOR -eq 5 -a $LIBWWW_MINOR -lt 2; then
            LIBWWW_VERSION_OK=no
        else
            if test $LIBWWW_MAJOR -eq 5 -a $LIBWWW_MINOR -eq 2 \
                -a $LIBWWW_MICRO -lt 8; then
                LIBWWW_VERSION_OK=no
            fi
        fi
    fi
    if test "x$LIBWWW_VERSION_OK" = "xno"; then
        AC_MSG_ERROR(w3c-libwww version >= 5.2.8 required)
    fi

    dnl Get the huge list of libraries we need to link against.
    LIBWWW_LDADD=$(LIBWWW_CONFIG --libs)
    AC_SUBST(LIBWWW_LDADD)

    AC_MSG_CHECKING(for libwww library directory)
    LIBWWW_LIBDIR="$(LIBWWW_CONFIG --prefix)/lib"
    AC_MSG_RESULT($LIBWWW_LIBDIR)
    AC_SUBST(LIBWWW_LIBDIR)

fi # MUST_BUILD_LIBWWW_CLIENT


dnl =======================================================================
dnl Finding Curl
dnl =======================================================================

if test $MUST_BUILD_CURL_CLIENT = yes; then

    dnl First of all, locate the Curl config program.
    dnl You can control which of these gets chosen by controlling PATH.
    AC_PATH_PROGS(CURL_CONFIG, curl-xmlrpc-config curl-config, no)
    if test "x$CURL_CONFIG" = "xno"; then
        AC_MSG_ERROR(Configure INTERNAL ERROR - first curl-config found, then not found)
    fi
    
    dnl There used to be code here to check the Curl version and make sure
    dnl it is at least 7.8.  But there were bugs both in the code and in 
    dnl curl (curl-config --vernum, at least in older versions of Curl,
    dnl omits the leading zero).  So it didn't work.  Plus, checking version
    dnl numbers isn't a good idea.  Better to check for feature presence.
    dnl So we don't do any check now.  If we find out there's a problem with
    dnl older Curls, we will revisit that.

    CURL_LDADD=$($CURL_CONFIG --libs)
    AC_SUBST(CURL_LDADD)

    AC_MSG_CHECKING(for Curl library directory)
    CURL_LIBDIR="$($CURL_CONFIG --prefix)/lib"

    AC_MSG_RESULT($CURL_LIBDIR)
    AC_SUBST(CURL_LIBDIR)

fi # MUST_BUILD_CURL_CLIENT


dnl =======================================================================
dnl Checks for build options.
dnl =======================================================================

AC_ARG_WITH(libwww-ssl,
  [  --with-libwww-ssl       Include libwww SSL capability.]
   ) 

if test x"$enable_libwww_client" != xno; then
    AC_MSG_CHECKING(whether to use SSL with libwww)
    if test x"$with_libwww_ssl" = xyes; then
        AC_MSG_RESULT(yes)
        HAVE_LIBWWW_SSL_DEFINE=1
    else
        AC_MSG_RESULT(no)
        HAVE_LIBWWW_SSL_DEFINE=0
    fi
fi
AC_SUBST(HAVE_LIBWWW_SSL_DEFINE)

dnl Check to see if we should build the libxml2 backend.
AC_ARG_ENABLE(libxml2-backend,
  [  --enable-libxml2-backend  Use libxml2 instead of built-in expat], ,
enable_libxml2_backend=no)
AC_MSG_CHECKING(whether to build the libxml2 backend)
AC_MSG_RESULT($enable_libxml2_backend)

if test $enable_libxml2_backend = yes; then
  AC_CHECK_PROG(have_xml2_config, xml2-config, yes, no)
  if test $have_xml2_config = no; then
    AC_MSG_ERROR([You specified --enable-libxml2_backend, but don't appear to have libxml2 installed (no working xml2-config in your command search path), so we cannot not build for libxml2])
  fi
fi
ENABLE_LIBXML2_BACKEND=$enable_libxml2_backend
AC_SUBST(ENABLE_LIBXML2_BACKEND)

dnl =======================================================================
dnl Compiler information
dnl =======================================================================
C_COMPILER_GNU=$ac_cv_c_compiler_gnu
AC_SUBST(C_COMPILER_GNU)
CXX_COMPILER_GNU=$ac_cv_cxx_compiler_gnu
AC_SUBST(CXX_COMPILER_GNU)

dnl obsolete variables, need to be removed from Makefile.in:
CC_WARN_FLAGS=
AC_SUBST(CC_WARN_FLAGS)
CPP_WARN_FLAGS=
AC_SUBST(CPP_WARN_FLAGS)


BUILDDIR=$(pwd)
AC_SUBST(BUILDDIR)


dnl =======================================================================
dnl Output our results.
dnl =======================================================================

dnl Note that AM_CONFIG_HEADER at the top of this file outputs another
dnl result: xmlrpc_amconfig.h .

AC_OUTPUT( \
          srcdir.mk \
          config.mk \
          xmlrpc_config.h \
          )


if test ! -f GNUmakefile; then
  ln -s "${srcdir}/GNUmakefile" .
fi
if test ! -f Makefile; then
  ln -s "${srcdir}/Makefile" .
fi


if test "$MUST_BUILD_WININET_CLIENT $MUST_BUILD_CURL_CLIENT $MUST_BUILD_LIBWWW_CLIENT" = "no no no"; then
  AC_MSG_NOTICE([==>])
  AC_MSG_NOTICE([==>We are not building any client XML transport (see earlier messages explaining why), therefore WE WILL NOT BUILD THE CLIENT LIBRARY.])
  AC_MSG_NOTICE([==>])
fi



dnl ======================================================================
dnl su module
dnl ======================================================================

AC_DEFUN([SAC_SU])

AC_DEFUN([SAC_SOFIA_SU], [
# Beginning of SAC_SOFIA_SU

# ======================================================================
# Check for features used by su

dnl Define compilation options for su_configure.h

case "$target" in
*-*-solaris?.* )
  SAC_SU_DEFINE(__EXTENSIONS__, 1, [Define to 1 in Solaris in order to get POSIX extensions.])
;;
esac

# Check includes used by su includes
AC_CHECK_HEADER(sys/types.h,
	SAC_SU_DEFINE([SU_HAVE_SYS_TYPES], 1,
		     [Define to 1 if Sofia uses sys/types.h]))

ax_inttypes=false
AC_CHECK_HEADER(stdint.h, [
	ax_inttypes=true
	SAC_SU_DEFINE([SU_HAVE_STDINT], 1,
		     [Define to 1 if Sofia uses stdint.h])])
AC_CHECK_HEADER(inttypes.h,[
	ax_inttypes=true
	SAC_SU_DEFINE([SU_HAVE_INTTYPES], 1,
		     [Define to 1 if Sofia uses inttypes.h])])

if $ax_inttypes; then : ; else
	AC_MSG_ERROR("No <stdint.h> or <inttypes.h> found.")
fi

if test "x$MINGW_ENVIRONMENT" != x1 ; then
  AC_CHECK_HEADER(pthread.h,
        HAVE_PTHREADS=1;
	SAC_SU_DEFINE([SU_HAVE_PTHREADS], 1, [Sofia SU uses pthreads]))
else
  HAVE_PTHREADS=1;
  SAC_SU_DEFINE([SU_HAVE_PTHREADS], 1, [Sofia SU uses pthreads])
fi

AC_ARG_ENABLE(experimental,
[  --enable-experimental   enable experimental features [[disabled]]],
 , enable_experimental=no)

if test $enable_experimental = yes ; then
  SAC_SU_DEFINE([SU_HAVE_EXPERIMENTAL], 1, [Enable experimental features])
fi

dnl ===========================================================================
dnl Checks for typedefs, headers, structures, and compiler characteristics.
dnl ===========================================================================

AC_REQUIRE([AC_C_CONST])
AC_REQUIRE([AC_HEADER_TIME])
AC_REQUIRE([AC_TYPE_SIZE_T])
AC_REQUIRE([AC_C_VAR_FUNC])
AC_REQUIRE([AC_C_MACRO_FUNCTION])

AC_REQUIRE([AC_C_INLINE])

AC_ARG_ENABLE(tag-cast,
[  --disable-tag-cast      cast tag values with inlined functions [[enabled]]],
 , enable_tag_cast=yes)

if test "$enable_tag_cast" = "yes"; then
    tag_cast=1
else
    tag_cast=0
fi

case "$ac_cv_c_inline" in
  yes) SAC_SU_DEFINE(su_inline, static inline, [
		Define to declarator for static inline functions.
	])dnl
       SAC_SU_DEFINE(SU_INLINE, inline, [
		Define to declarator for inline functions.
	])dnl
       SAC_SU_DEFINE(SU_HAVE_INLINE, 1, [
		Define to 1 if you have inline functions.
	])dnl
       SAC_SU_DEFINE_UNQUOTED(SU_INLINE_TAG_CAST, $tag_cast, [
		Define to 1 if you use inline function to cast tag values.
	])dnl
  ;;
  no | "" )
       SAC_SU_DEFINE(su_inline, static)dnl
       SAC_SU_DEFINE(SU_INLINE, /*inline*/)dnl
       SAC_SU_DEFINE(SU_HAVE_INLINE, 0)dnl
       SAC_SU_DEFINE(SU_INLINE_TAG_CAST, 0)dnl
  ;;
  *)   SAC_SU_DEFINE_UNQUOTED(su_inline, static $ac_cv_c_inline)dnl
       SAC_SU_DEFINE_UNQUOTED(SU_INLINE, $ac_cv_c_inline)dnl
       SAC_SU_DEFINE(SU_HAVE_INLINE, 1)dnl
       SAC_SU_DEFINE_UNQUOTED(SU_INLINE_TAG_CAST, $tag_cast)dnl
  ;;
esac

AC_ARG_ENABLE(size-compat,
[  --disable-size-compat   use compatibility size_t types [[enabled]]],
 , enable_size_compat=yes)

if test X$enable_size_compat != Xyes; then
       SAC_SU_DEFINE(SOFIA_ISIZE_T, size_t)dnl
       SAC_SU_DEFINE(ISIZE_MAX, SIZE_MAX)dnl
       SAC_SU_DEFINE(SOFIA_ISSIZE_T, ssize_t)dnl
       SAC_SU_DEFINE(ISSIZE_MAX, SSIZE_MAX)dnl
       SAC_SU_DEFINE(SOFIA_USIZE_T, size_t)dnl
       SAC_SU_DEFINE(USIZE_MAX, SIZE_MAX)dnl
else
       SAC_SU_DEFINE(SOFIA_ISIZE_T, int)dnl
       SAC_SU_DEFINE(ISIZE_MAX, INT_MAX)dnl
       SAC_SU_DEFINE(SOFIA_ISSIZE_T, int)dnl
       SAC_SU_DEFINE(ISSIZE_MAX, INT_MAX)dnl
       SAC_SU_DEFINE(SOFIA_USIZE_T, unsigned)dnl
       SAC_SU_DEFINE(USIZE_MAX, UINT_MAX)dnl
fi


dnl ======================================================================
dnl SAC_ENABLE_COREFOUNDATION
dnl ======================================================================
AC_ARG_ENABLE(corefoundation,
[  --enable-corefoundation
                          compile with OSX COREFOUNDATION [[disabled]]],
 , enable_corefoundation=no)
AM_CONDITIONAL(COREFOUNDATION, test $enable_corefoundation = yes)

if test $enable_corefoundation = yes ; then
   SAC_SU_DEFINE([SU_HAVE_OSX_CF_API], 1, [
Define as 1 if you have OSX CoreFoundation interface])
   LIBS="-framework CoreFoundation -framework SystemConfiguration $LIBS"
fi


### ======================================================================
### Test if we have stack suitable for handling tags directly
###

AC_CACHE_CHECK([for stack suitable for tags],[ac_cv_tagstack],[
ac_cv_tagstack=no

AC_RUN_IFELSE([AC_LANG_SOURCE([[
#if HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#if HAVE_STDINT_H
#include <stdint.h>
#endif

#include <stdarg.h>

typedef void *tp;
typedef intptr_t tv;

int test1(tv l, tv h, ...)
{
  va_list ap;
  tv i, *p = &l;

  va_start(ap, h);

  if (*p++ != l || *p++ != h) return 1;

  for (i = l; i <= h; i++) {
    if (*p++ != i)
      return 1;
  }

  for (i = l; i <= h; i++) {
    if (va_arg(ap, tv) != i)
      return 1;
  }

  va_end(ap);

  return 0;
}

int main(int avc, char **av)
{
  return test1((tv)1, (tv)10,
	       (tv)1, (tv)2, (tv)3, (tv)4, (tv)5,
	       (tv)6, (tv)7, (tv)8, (tv)9, (tv)10);
}
]])],[ac_cv_tagstack=yes],[ac_cv_tagstack=no],[
case "$target" in
i?86-*-* ) ac_cv_tagstack=yes ;;
* ) ac_cv_tagstack=no ;;
esac
])])

if test $ac_cv_tagstack = yes ; then
SAC_SU_DEFINE([SU_HAVE_TAGSTACK], 1, [
Define this as 1 if your compiler puts the variable argument list nicely in memory])
fi

### ======================================================================
### Test if free(0) fails
###

AC_CACHE_CHECK([for graceful free(0)],[ac_cv_free_null],[
ac_cv_free_null=no

AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdlib.h>

int main(int avc, char **av)
{
	free(0);
	return 0;
}
]])],[ac_cv_free_null=yes],[ac_cv_free_null=no],[ac_cv_free_null=no])])

if test $ac_cv_free_null = yes ; then
AC_DEFINE([HAVE_FREE_NULL], 1, [
Define this as 1 if your c library does not crash with free(0)])
fi

dnl ======================================================================
dnl Socket features

AC_REQUIRE([AC_SYS_SA_LEN])
if test "$ac_cv_sa_len" = yes ;then
  SAC_SU_DEFINE([SU_HAVE_SOCKADDR_SA_LEN], 1,
	        [Define to 1 if you have sa_len in struct sockaddr])
fi

AC_ARG_ENABLE([ip6],
[  --disable-ip6          disable IPv6 functionality [[enabled]]],,enable_ip6=yes)

if ! test no$enable_ip6 = nono ; then
AC_STRUCT_SIN6
case $ac_cv_sin6 in
yes) SAC_SU_DEFINE(SU_HAVE_IN6, 1, [
	Define to 1 if you have struct sockaddr_in6]) ;;
 no) ;;
  *) AC_MSG_ERROR([Inconsistent struct sockaddr_sin6 test]) ;;
esac
fi

AC_CHECK_HEADERS([unistd.h sys/time.h])

AC_CHECK_HEADERS([fcntl.h dirent.h])

AC_CHECK_HEADERS([winsock2.h], [
  AC_DEFINE([HAVE_WIN32], 1, [Define to 1 you have WIN32])
  SAC_SU_DEFINE([SU_HAVE_WINSOCK], 1, [Define to 1 you have WinSock])
  SAC_SU_DEFINE([SU_HAVE_WINSOCK2], 1, [Define to 1 you have WinSock2])
  SAC_SU_DEFINE([SU_HAVE_SOCKADDR_STORAGE], 1,
      [Define to 1 if you have struct sockaddr_storage])
  AC_DEFINE([HAVE_ADDRINFO], 1,
      [Define to 1 if you have addrinfo structure.])
  AC_DEFINE([HAVE_GETADDRINFO], 1,
      [Define to 1 if you have addrinfo structure.])
  AC_DEFINE([HAVE_FREEADDRINFO], 1,
      [Define to 1 if you have addrinfo structure.])
  SAC_SU_DEFINE([SU_HAVE_ADDRINFO], 1,
      [Define to 1 if you have addrinfo structure.])
  AC_CHECK_HEADERS([windef.h ws2tcpip.h])
  AC_CHECK_HEADERS([iphlpapi.h], [
    AC_DEFINE([HAVE_INTERFACE_INFO_EX], 1, [
       Define to 1 if you have WIN32 INTERFACE_INFO_EX type.])
    AC_DEFINE([HAVE_SIO_ADDRESS_LIST_QUERY], 1, [
       Define to 1 if you have WIN32 WSAIoctl SIO_ADDRESS_LIST_QUERY.])
  ], [], [#if HAVE_WINDEF_H
#include <windef.h>
#include <winbase.h>
#endif
  ])
  AC_DEFINE([HAVE_FILETIME], 1, [
     Define to 1 if you have WIN32 FILETIME type and
     GetSystemTimeAsFileTime().])
],[
dnl no winsock2

SAC_SU_DEFINE([SU_HAVE_BSDSOCK], 1, [Define to 1 if you have BSD socket interface])
AC_CHECK_HEADERS([sys/socket.h sys/ioctl.h sys/filio.h sys/sockio.h \
		  sys/select.h sys/epoll.h sys/devpoll.h])
AC_CHECK_HEADERS([netinet/in.h arpa/inet.h netdb.h \
                  net/if.h net/if_types.h ifaddr.h netpacket/packet.h],,,
		[
#include <sys/types.h>
#include <sys/socket.h>])

AC_CHECK_DECL([MSG_TRUNC],
AC_DEFINE([HAVE_MSG_TRUNC],1,[Define to 1 if you have MSG_TRUNC flag]),,[
#include <sys/types.h>
#include <sys/socket.h>])

AC_CHECK_DECL([SO_RCVBUFFORCE],
AC_DEFINE([HAVE_SO_RCVBUFFORCE],1,[Define to 1 if you have socket option SO_RCVBUFFORCE]),,[
#include <sys/types.h>
#include <sys/socket.h>])

AC_CHECK_DECL([SO_SNDBUFFORCE],
AC_DEFINE([HAVE_SO_SNDBUFFORCE],1,[Define to 1 if you have socket option SO_SNDBUFFORCE]),,[
#include <sys/types.h>
#include <sys/socket.h>])

AC_CHECK_DECL([IP_ADD_MEMBERSHIP],
AC_DEFINE([HAVE_IP_ADD_MEMBERSHIP],1,[Define to 1 if you have IP_ADD_MEMBERSHIP]),,[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>])

AC_CHECK_DECL([IP_MULTICAST_LOOP],
AC_DEFINE([HAVE_IP_MULTICAST_LOOP],1,[Define to 1 if you have IP_MULTICAST_LOOP]),,[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>])

AC_CHECK_DECL([IP_MTU_DISCOVER],
AC_DEFINE([HAVE_IP_MTU_DISCOVER],1,
[Define to 1 if you have IP_MTU_DISCOVER]),,[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>])

AC_CACHE_CHECK([for struct addrinfo],
[ac_cv_struct_addrinfo],[
ac_cv_struct_addrinfo=no
if test "$ac_cv_header_sys_socket_h" = yes; then
  AC_EGREP_HEADER([struct.+addrinfo], [netdb.h], [
  ac_cv_struct_addrinfo=yes])
else
  ac_cv_struct_addrinfo='sys/socket.h missing'
fi])

if test "$ac_cv_struct_addrinfo" = yes; then
  SAC_SU_DEFINE([SU_HAVE_ADDRINFO], 1,
    [Define to 1 if you have struct addrinfo.])
fi

AC_CACHE_CHECK([for struct sockaddr_storage],
[ac_cv_struct_sockaddr_storage],[
ac_cv_struct_sockaddr_storage=no
if test "$ac_cv_header_sys_socket_h" = yes; then
  AC_EGREP_HEADER([struct.+sockaddr_storage], [sys/socket.h], [
  ac_cv_struct_sockaddr_storage=yes])
else
  ac_cv_struct_sockaddr_storage='sys/socket.h missing'
fi])
if test "$ac_cv_struct_sockaddr_storage" = yes; then
  SAC_SU_DEFINE(SU_HAVE_SOCKADDR_STORAGE, 1,
    [Define to 1 if you have struct sockaddr_storage])
fi

AC_CACHE_CHECK([for field ifr_index in struct ifreq],
[ac_cv_struct_ifreq_ifr_index],[
ac_cv_struct_ifreq_ifr_index=no
if test "1${ac_cv_header_arpa_inet_h}2${ac_cv_header_netdb_h}3${ac_cv_header_sys_socket_h}4${ac_cv_header_net_if_h}" = 1yes2yes3yes4yes; then
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <net/if.h>]], [[
struct ifreq ifreq; int index; index = ifreq.ifr_index;
]])],[ac_cv_struct_ifreq_ifr_index=yes],[])
else
  ac_cv_struct_ifreq_ifr_index='net/if.h missing'
fi # arpa/inet.h && netdb.h && sys/socket.h && net/if.h
])
if test "$ac_cv_struct_ifreq_ifr_index" = yes ; then
  :
  AC_DEFINE(HAVE_IFR_INDEX, 1, [Define to 1 if you have ifr_index in <net/if.h>])
else
AC_CACHE_CHECK([for field ifr_ifindex in struct ifreq],
[ac_cv_struct_ifreq_ifr_ifindex],[
ac_cv_struct_ifreq_ifr_ifindex=no
if test "1${ac_cv_header_arpa_inet_h}2${ac_cv_header_netdb_h}3${ac_cv_header_sys_socket_h}4${ac_cv_header_net_if_h}" = 1yes2yes3yes4yes; then
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <net/if.h>]], [[
struct ifreq ifreq; int index; index = ifreq.ifr_ifindex;
]])],[ac_cv_struct_ifreq_ifr_ifindex=yes],[])
else
  ac_cv_struct_ifreq_ifr_ifindex='net/if.h missing'
fi # arpa/inet.h && netdb.h && sys/socket.h && net/if.h
])
if test "$ac_cv_struct_ifreq_ifr_ifindex" = yes; then
  :
  AC_DEFINE(HAVE_IFR_IFINDEX, 1, [Define to 1 if you have ifr_ifindex in <net/if.h>])
fi

fi # ifr_index in struct ifreq

dnl SIOGCIFCONF & struct ifconf
AC_CACHE_CHECK([for struct ifconf],
[ac_cv_struct_ifconf],[
ac_cv_struct_ifconf=no
if test "$ac_cv_header_net_if_h" = yes; then
  AC_EGREP_HEADER(struct.+ifconf, net/if.h, ac_cv_struct_ifconf=yes)
else
  ac_cv_struct_ifconf='net/if.h missing'
fi])
if test "$ac_cv_struct_ifconf" = yes; then
  AC_DEFINE(HAVE_IFCONF, 1, [Define to 1 if you have SIOCGIFCONF])
fi

AC_CACHE_CHECK([for ioctl SIOCGIFNUM],
[ac_cv_ioctl_siocgifnum],[
ac_cv_ioctl_siocgifnum=no
if test "$ac_cv_header_sys_sockio_h" = yes; then
AC_EGREP_CPP(yes, [
#include <sys/sockio.h>
#ifdef SIOCGIFNUM
  yes
#endif
], [ac_cv_ioctl_siocgifnum=yes])
else
  ac_cv_ioctl_siocgifnum='sys/sockio.h missing'
fi])
if test "$ac_cv_ioctl_siocgifnum" = yes; then
  HAVE_IFNUM=1
  AC_DEFINE(HAVE_IFNUM, 1, [Define to 1 if you have SIOCGIFNUM ioctl])
else
  HAVE_IFNUM=0
fi

]) dnl AC_CHECK_HEADERS([winsock2.h ... ])


# ===========================================================================
# Checks for libraries
# ===========================================================================

AC_CHECK_LIB(pthread, pthread_create)
AC_CHECK_LIB(socket, socketpair,,,-lnsl)

AC_ARG_WITH(rt,
[  --with-rt               use POSIX realtime library [[used by default]]])
if test "${with_rt}" != no; then
	AC_SEARCH_LIBS(clock_gettime, rt)
        AC_CHECK_FUNCS([clock_gettime clock_getcpuclockid])
        AC_CHECK_DECL([CLOCK_MONOTONIC],
AC_DEFINE([HAVE_CLOCK_MONOTONIC], 1,
[Define to 1 if you have CLOCK_MONOTONIC]),,[
#include <time.h>])
fi

# No GLib path explicitly defined, use pkg-config
AC_ARG_WITH(glib,
[  --with-glib=version     use GLib (default=2.0)], [
case "$with_glib" in
yes | "" ) with_glib=2.0 ;;
esac
], [with_glib=2.0])

AC_ARG_WITH(glib-dir,
[  --with-glib-dir=PREFIX  explicitly define GLib path],,
 with_glib_dir="pkg-config")

if test "$with_glib" = no || test "$with_glib_dir" = "no" ; then

  : # No glib

elif test "$with_glib_dir" = "pkg-config" ; then

  PKG_CHECK_MODULES(GLIB, glib-$with_glib, [HAVE_GLIB=yes], [HAVE_GLIB=no])

else # GLib path is explicitly defined

  gprefix=$with_glib_dir
  GLIB_VERSION="$with_glib"
  GLIBXXX=glib-$with_glib

  if test "$gprefix" = "yes" ; then
    for gprefix in /usr /usr/local /opt/$GLIBXXX
    do
  	test -d $gprefix/include/$GLIBXXX && break
    done
  fi

  if ! test -d $gprefix/include/$GLIBXXX ; then
    AC_MSG_ERROR("No $GLIBXXX in --with-glib=$with_glib_dir")
  else
    exec_gprefix=${gprefix}
    glibdir=${exec_gprefix}/lib
    gincludedir=${gprefix}/include

    # glib_genmarshal=glib-genmarshal
    # glib_mkenums=glib-mkenums

    HAVE_GLIB=yes

    if test "x$MINGW_ENVIRONMENT" = x1 ; then
      GLIB_LIBS="${glibdir}/lib$GLIBXXX.dll.a ${glibdir}/libintl.a ${glibdir}/libiconv.a"
    else
      GLIB_LIBS="-L${glibdir} -l$GLIBXXX -lintl -liconv"
    fi
    GLIB_CFLAGS="-I${gincludedir}/$GLIBXXX -I${glibdir}/$GLIBXXX/include"
  fi

fi # GLib path is explicitly defined

if test "x$HAVE_GLIB" = xyes ; then
  SAC_COMMA_APPEND([SOFIA_GLIB_PKG_REQUIRES],[glib-2.0])
fi

AM_CONDITIONAL([HAVE_GLIB], [test "x$HAVE_GLIB" = xyes])
AC_SUBST([GLIB_LIBS])
AC_SUBST([GLIB_CFLAGS])
AC_SUBST([GLIB_VERSION])
AC_SUBST([SOFIA_GLIB_PKG_REQUIRES])

# ===========================================================================
# Checks for library functions.
# ===========================================================================

AC_SEARCH_LIBS(socket, xnet socket)
AC_SEARCH_LIBS(inet_ntop, socket nsl)
dnl AC_SEARCH_LIBS(inet_pton, socket nsl)
AC_SEARCH_LIBS(getipnodebyname, xnet socket nsl)
AC_SEARCH_LIBS(gethostbyname, xnet nsl)
AC_SEARCH_LIBS(getaddrinfo, xnet socket nsl)

AC_FUNC_ALLOCA

AC_CHECK_FUNCS([gettimeofday strerror random initstate tcsetattr flock \
                socketpair gethostname gethostbyname getipnodebyname \
                poll epoll_create kqueue select if_nameindex \
		signal alarm \
		strnlen \
	        getaddrinfo getnameinfo freeaddrinfo gai_strerror getifaddrs \
                getline getdelim getpass])
# getline getdelim getpass are _GNU_SOURCE stuff

if test $ac_cv_func_poll = yes ; then
  SAC_SU_DEFINE([SU_HAVE_POLL], 1, [Define to 1 if you have poll().])
fi

if test $ac_cv_func_epoll_create = yes && test $ac_cv_header_sys_epoll_h = yes
then
  AC_DEFINE([HAVE_EPOLL], 1, [Define to 1 if you have epoll interface.])
fi

if test $ac_cv_func_if_nameindex = yes ; then
  SAC_SU_DEFINE([SU_HAVE_IF_NAMEINDEX], 1,
    [Define to 1 if you have if_nameindex().])
fi

SAC_REPLACE_FUNCS([memmem memccpy memspn memcspn strtoull \
		   inet_ntop inet_pton poll])

if test $ac_cv_func_signal = yes ; then
AC_CHECK_DECL([SIGPIPE], [
AC_DEFINE([HAVE_SIGPIPE], 1, [Define to 1 if you have SIGPIPE])],,[
#include <signal.h>
])
dnl add SIGHUP SIGQUIT if needed
fi

# ===========================================================================
# Check how to implement su_port
# ===========================================================================

AC_ARG_ENABLE(poll-port,
[  --disable-poll-port     disable su_poll_port [[enabled]]
                          Use this option in systems emulating poll with select],
 , enable_poll_port=maybe)

if test $enable_poll_port = maybe ; then
  if test $ac_cv_func_poll = yes ; then
    AC_DEFINE([HAVE_POLL_PORT], 1, [Define to 1 if you use poll in su_port.])
  fi
elif test $enable_poll_port = yes ; then
    AC_DEFINE([HAVE_POLL_PORT], 1, [Define to 1 if you use poll in su_port.])
fi

# ===========================================================================
# Check pthread_rwlock_unlock()
# ===========================================================================

AC_DEFUN([AC_DEFINE_HAVE_PTHREAD_RWLOCK],[dnl
AC_DEFINE([HAVE_PTHREAD_RWLOCK], 1,[
Define to 1 if you have working pthread_rwlock_t implementation.

   A  thread  may hold multiple concurrent read locks on rwlock - that is,
   successfully call the pthread_rwlock_rdlock() function  n  times.  If
   so,  the  application  shall  ensure that the thread performs matching
   unlocks - that is, it  calls  the  pthread_rwlock_unlock()  function  n
   times.
])])

if test x$HAVE_PTHREADS = x1 ; then

AC_RUN_IFELSE([
#define _XOPEN_SOURCE (500)

#include <pthread.h>

pthread_rwlock_t rw;

int main()
{
  pthread_rwlock_init(&rw, NULL);
  pthread_rwlock_rdlock(&rw);
  pthread_rwlock_rdlock(&rw);
  pthread_rwlock_unlock(&rw);
  /* pthread_rwlock_trywrlock() should fail (not return 0) */
  return pthread_rwlock_trywrlock(&rw) != 0 ? 0  : 1;
}
],[AC_DEFINE_HAVE_PTHREAD_RWLOCK],[
AC_MSG_WARN([Recursive pthread_rwlock_rdlock() does not work!!! ])
],[AC_DEFINE_HAVE_PTHREAD_RWLOCK])

fi

# ===========================================================================
# Check IPv6 addresss configuration
# ===========================================================================
case "$target" in
 *-*-linux*) AC_DEFINE([HAVE_PROC_NET_IF_INET6], 1,
	[Define to 1 if you have /proc/net/if_inet6 control file]) ;;
esac

AC_CONFIG_HEADERS([libsofia-sip-ua/su/sofia-sip/su_configure.h])

# End of SAC_SOFIA_SU
])

dnl
dnl Append a value $2 to a variable, separating values with comma
dnl
AC_DEFUN([SAC_COMMA_APPEND],[$1="`test -n "$$1" && echo "$$1, "`$2"])

# SAC_SU_DEFINE(VARIABLE, [VALUE], [DESCRIPTION])
# -------------------------------------------
# Set VARIABLE to VALUE, verbatim, or 1.  Remember the value
# and if VARIABLE is affected the same VALUE, do nothing, else
# die.  The third argument is used by autoheader.

m4_define([SAC_SU_DEFINE],[
cat >>confdefs.h <<\_AXEOF
[@%:@define] $1 m4_if($#, 2, [$2], $#, 3, [$2], 1)
_AXEOF
])

# SAC_SU_DEFINE_UNQUOTED(VARIABLE, [VALUE], [DESCRIPTION])
# ----------------------------------------------------
# Similar, but perform shell substitutions $ ` \ once on VALUE.
m4_define([SAC_SU_DEFINE_UNQUOTED],[
cat >>confdefs.h <<_ACEOF
[@%:@define] $1 m4_if($#, 2, [$2], $#, 3, [$2], 1)
_ACEOF
])

AC_DEFUN([SAC_REPLACE_FUNCS],[dnl
AC_CHECK_FUNCS([$1],ifelse([$2], , :,[$2]),[dnl
case "$REPLACE_LIBADD" in
    "$ac_func.lo"   | \
  *" $ac_func.lo"   | \
    "$ac_func.lo "* | \
  *" $ac_func.lo "* ) ;;
  *) REPLACE_LIBADD="$REPLACE_LIBADD $ac_func.lo" ;;
esac])
AC_SUBST([REPLACE_LIBADD])
ifelse([$3], , :, [$3])
])

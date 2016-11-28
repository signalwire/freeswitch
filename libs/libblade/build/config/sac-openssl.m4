dnl ======================================================================
dnl SAC_OPENSSL
dnl ======================================================================
AC_DEFUN([SAC_OPENSSL], [

AC_ARG_WITH(openssl,
[  --with-openssl          use OpenSSL [[enabled]]],, with_openssl=pkg-config)

dnl SOSXXX:SAC_ASSERT_DEF([openssl libraries])


if test "$with_openssl" = no  ;then
  : # No openssl
else

  if test "$with_openssl" = "pkg-config" ; then
    PKG_CHECK_MODULES(openssl, openssl,
	[HAVE_TLS=1 HAVE_OPENSSL=1 LIBS="$openssl_LIBS $LIBS"],
	[HAVE_OPENSSL=0])
  fi

  if test x$HAVE_OPENSSL = x1 ; then
     AC_DEFINE([HAVE_LIBCRYPTO], 1, [Define to 1 if you have the `crypto' library (-lcrypto).])
     AC_DEFINE([HAVE_LIBSSL], 1, [Define to 1 if you have the `ssl' library (-lssl).])
  else
    AC_CHECK_HEADERS([openssl/tls1.h], [
      HAVE_OPENSSL=1 HAVE_TLS=1

      AC_CHECK_LIB(crypto, BIO_new,,
      	HAVE_OPENSSL=0
      	AC_MSG_WARN(OpenSSL crypto library was not found))

      AC_CHECK_LIB(ssl, TLSv1_method,,
      	HAVE_TLS=0
      	AC_MSG_WARN(OpenSSL protocol library was not found))
     ],[AC_MSG_WARN(OpenSSL include files were not found)],[#include <openssl/safestack.h>])
  fi

  if test x$HAVE_OPENSSL = x1; then
     AC_DEFINE([HAVE_OPENSSL], 1, [Define to 1 if you have OpenSSL])
  fi

  if test x$HAVE_TLS = x1; then
    AC_DEFINE([HAVE_TLS], 1, [Define to 1 if you have TLS])
  fi
fi

AM_CONDITIONAL(HAVE_TLS, test x$HAVE_TLS = x1)
])

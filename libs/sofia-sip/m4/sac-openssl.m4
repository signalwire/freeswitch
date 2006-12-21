dnl ======================================================================
dnl SAC_OPENSSL
dnl ======================================================================
AC_DEFUN([SAC_OPENSSL], [

AC_ARG_WITH(openssl,
[  --with-openssl          use OpenSSL (enabled)],, with_openssl=yes)

dnl SOSXXX:SAC_ASSERT_DEF([openssl libraries])

if test "$with_openssl" != no  ;then
  AC_CHECK_HEADERS(openssl/tls1.h, [

    HAVE_OPENSSL=1 HAVE_TLS=1

    AC_CHECK_LIB(crypto, BIO_new,, 
    	HAVE_OPENSSL=0
    	AC_MSG_WARN(OpenSSL crypto library was not found))

    AC_CHECK_LIB(ssl, TLSv1_method,, 
    	HAVE_TLS=0
    	AC_MSG_WARN(OpenSSL protocol library was not found))

    if test x$HAVE_OPENSSL = x1; then
      AC_DEFINE([HAVE_OPENSSL], 1, [Define to 1 if you have OpenSSL])
    fi

    if test x$HAVE_TLS = x1; then
      AC_DEFINE([HAVE_TLS], 1, [Define to 1 if you have TLS])
    fi
  ],
  AC_MSG_WARN(OpenSSL include files were not found))
fi

AM_CONDITIONAL(HAVE_TLS, test x$HAVE_TLS = x1)
])

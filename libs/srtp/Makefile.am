AUTOMAKE_OPTIONS = gnu subdir-objects
NAME=srtp

AM_CFLAGS    = $(new_AM_CFLAGS) -I./src -Icrypto/include -I$(srcdir)/include -I$(srcdir)/crypto/include
AM_CPPFLAGS  = $(AM_CFLAGS)
AM_LDFLAGS   = $(new_AM_LDFLAGS) -L.

HMAC_OBJS = @HMAC_OBJS@
AES_ICM_OBJS = @AES_ICM_OBJS@

lib_LTLIBRARIES = libsrtp.la
libsrtp_la_SOURCES =    srtp/srtp.c srtp/ekt.c crypto/cipher/cipher.c crypto/cipher/null_cipher.c \
			crypto/hash/null_auth.c crypto/hash/auth.c \
			crypto/math/datatypes.c crypto/math/stat.c \
			crypto/kernel/crypto_kernel.c crypto/kernel/alloc.c \
			crypto/kernel/key.c \
			crypto/kernel/err.c \
			crypto/replay/rdb.c crypto/replay/rdbx.c crypto/replay/ut_sim.c 

libsrtp_la_LDFLAGS = 	-version-info 1:42:1

EXTRA_DIST=

if ENABLE_OPENSSL
libsrtp_la_SOURCES += crypto/cipher/aes_icm_ossl.c crypto/cipher/aes_gcm_ossl.c
libsrtp_la_SOURCES += crypto/hash/hmac_ossl.c
else
libsrtp_la_SOURCES += crypto/hash/sha1.c crypto/hash/hmac.c 
libsrtp_la_SOURCES += crypto/cipher/aes_icm.c crypto/cipher/aes.c
endif

if GDOI
libsrtp_la_SOURCES += gdoi/srtp+gdoi.c
endif

library_includedir =  $(prefix)/include/srtp
library_include_HEADERS = include/rtp.h include/srtp.h include/ut_sim.h crypto/include/auth.h \
          crypto/include/datatypes.h crypto/include/integers.h crypto/include/null_cipher.h \
          crypto/include/rdbx.h crypto/include/aes_icm.h crypto/include/cipher.h crypto/include/crypto_types.h  \
          crypto/include/err.h crypto/include/sha1.h \
          crypto/include/aes.h crypto/include/config.h crypto/include/key.h \
          crypto/include/stat.h crypto/include/alloc.h crypto/include/crypto_kernel.h \
          crypto/include/hmac.h crypto/include/null_auth.h crypto/include/rdb.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = srtp-1.42.pc

AUTOMAKE_OPTIONS = gnu
NAME=srtp

AM_CFLAGS    = $(new_AM_CFLAGS) -I./src -Icrypto/include -I$(srcdir)/include -I$(srcdir)/crypto/include
AM_CPPFLAGS  = $(AM_CFLAGS)
AM_LDFLAGS   = $(new_AM_LDFLAGS) -L.

HMAC_OBJS = @HMAC_OBJS@
RNG_EXTRA_OBJS = @RNG_EXTRA_OBJS@
AES_ICM_OBJS = @AES_ICM_OBJS@

lib_LTLIBRARIES = libsrtp.la libcryptomath.la
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
libsrtp_la_SOURCES += crypto/rng/rand_source_ossl.c
libsrtp_la_SOURCES += crypto/hash/hmac_ossl.c
else
libsrtp_la_SOURCES += crypto/hash/sha1.c crypto/hash/hmac.c 
libsrtp_la_SOURCES += crypto/cipher/aes_icm.c crypto/cipher/aes.c crypto/cipher/aes_cbc.c
libsrtp_la_SOURCES += crypto/rng/prng.c crypto/rng/ctr_prng.c
if RNG_OBJS_LINUX
libsrtp_la_SOURCES += crypto/rng/rand_linux_kernel.c
EXTRA_DIST += crypto/rng/rand_source.c
else
libsrtp_la_SOURCES += crypto/rng/rand_source.c
endif
endif

if GDOI
libsrtp_la_SOURCES += gdoi/srtp+gdoi.c
endif

libcryptomath_la_SOURCES = crypto/math/math.c crypto/math/gf2_8.c
libcryptomath_la_LDFLAGS = -version-info 1:42:1

library_includedir =  $(prefix)/include/srtp
library_include_HEADERS = include/rtp.h include/srtp.h include/ut_sim.h crypto/include/aes_cbc.h crypto/include/auth.h \
          crypto/include/crypto_math.h crypto/include/datatypes.h crypto/include/integers.h crypto/include/null_cipher.h \
          crypto/include/rdbx.h crypto/include/aes_icm.h crypto/include/cipher.h crypto/include/crypto_types.h  \
          crypto/include/err.h crypto/include/kernel_compat.h crypto/include/prng.h crypto/include/sha1.h \
          crypto/include/aes.h crypto/include/config.h crypto/include/crypto.h crypto/include/gf2_8.h  crypto/include/key.h \
          crypto/include/rand_source.h crypto/include/stat.h crypto/include/alloc.h crypto/include/crypto_kernel.h \
          crypto/include/cryptoalg.h crypto/include/hmac.h crypto/include/null_auth.h crypto/include/rdb.h crypto/include/xfm.h

noinst_PROGRAMS = aes_tables
aes_tables_SOURCES = tables/aes_tables.c
aes_tables_LDADD = libcryptomath.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = srtp-1.42.pc

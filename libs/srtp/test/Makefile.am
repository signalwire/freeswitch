AUTOMAKE_OPTS= gnu
AM_CFLAGS    = $(new_AM_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/crypto/include
AM_CPPFLAGS  = $(AM_CFLAGS)
AM_LDFLAGS   = $(new_AM_LDFLAGS) -L$(srcdir) -lsrtp

check_PROGRAMS = cipher_driver kernel_driver rdbx_driver srtp_driver roc_driver replay_driver \
		  aes_calc datatypes_driver rand_gen sha1_driver stat_driver rtpw

cipher_driver_SOURCES = crypto/cipher_driver.c
kernel_driver_SOURCES = crypto/kernel_driver.c
aes_calc_SOURCES = crypto/aes_calc.c
datatypes_driver_SOURCES = crypto/datatypes_driver.c
rand_gen_SOURCES =  crypto/rand_gen.c
sha1_driver_SOURCES = crypto/sha1_driver.c
stat_driver_SOURCES = crypto/stat_driver.c
rdbx_driver_SOURCES = rdbx_driver.c
srtp_driver_SOURCES = srtp_driver.c
roc_driver_SOURCES = roc_driver.c
replay_driver_SOURCES = replay_driver.c
rtpw_SOURCES = rtpw.c rtp.c
rtpw_LDADD = -lnsl -lsocket

check-local: cipher_driver
	./cipher_driver -v
	./kernel_driver -v
	./rdbx_driver -v
	./srtp_driver -v
	./roc_driver -v
	./replay_driver -v
	./datatypes_driver -v
	./stat_driver
	./sha1_driver -v
	./kernel_driver -v
	./rand_gen -n 256


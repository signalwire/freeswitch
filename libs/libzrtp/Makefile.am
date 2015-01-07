#
# Copyright (c) 2006-2007 Philip R. Zimmermann. All rights reserved.
# Contact: http://philzimmermann.com
# 
# Viktor Krikun <v.krikun@soft-industry.com> <v.krikun@gmail.com>
#
AUTOMAKE_OPTIONS = subdir-objects

libzrtp_includedir=$(includedir)/libzrtp
libzrtp_include_HEADERS = \
					$(top_srcdir)/include/zrtp.h \
					$(top_srcdir)/include/zrtp_base.h \
					$(top_srcdir)/include/zrtp_config.h \
					$(top_srcdir)/include/zrtp_config_user.h \
					$(top_srcdir)/include/zrtp_config_unix.h \
					$(top_srcdir)/include/zrtp_crypto.h \
					$(top_srcdir)/include/zrtp_ec.h \
					$(top_srcdir)/include/zrtp_engine.h \
					$(top_srcdir)/include/zrtp_error.h \
					$(top_srcdir)/include/zrtp_iface.h \
					$(top_srcdir)/include/zrtp_iface_scheduler.h \
					$(top_srcdir)/include/zrtp_iface_cache.h \
					$(top_srcdir)/include/zrtp_iface_system.h \
					$(top_srcdir)/include/zrtp_legal.h \
					$(top_srcdir)/include/zrtp_list.h \
					$(top_srcdir)/include/zrtp_log.h \
					$(top_srcdir)/include/zrtp_pbx.h \
					$(top_srcdir)/include/zrtp_protocol.h \
					$(top_srcdir)/include/zrtp_srtp.h \
					$(top_srcdir)/include/zrtp_srtp_builtin.h \
					$(top_srcdir)/include/zrtp_string.h \
					$(top_srcdir)/include/zrtp_types.h \
					$(top_srcdir)/include/zrtp_version.h \
					\
					$(top_srcdir)/third_party/bnlib/bn.h \
					\
					$(top_srcdir)/third_party/bgaes/aes.h \
					$(top_srcdir)/third_party/bgaes/aesopt.h \
					$(top_srcdir)/third_party/bgaes/aestab.h \
					$(top_srcdir)/third_party/bgaes/bg2zrtp.h \
					$(top_srcdir)/third_party/bgaes/brg_types.h \
					$(top_srcdir)/third_party/bgaes/sha1.h \
					$(top_srcdir)/third_party/bgaes/sha2.h

lib_LIBRARIES = libzrtp.a

libzrtp_a_CPPFLAGS = \
			-I$(top_srcdir)/include \
			-I$(top_srcdir)/. \
			-I$(top_srcdir)/third_party/bgaes \
			-I$(top_srcdir)/third_party/bnlib

libzrtp_a_LIBADD = $(top_srcdir)/third_party/bnlib/libbn.a

libzrtp_a_SOURCES = $(top_srcdir)/src/zrtp.c \
					$(top_srcdir)/src/zrtp_crc.c \
					$(top_srcdir)/src/zrtp_crypto_aes.c \
					$(top_srcdir)/src/zrtp_crypto_atl.c \
					$(top_srcdir)/src/zrtp_crypto_ec.c \
					$(top_srcdir)/src/zrtp_crypto_ecdh.c \
					$(top_srcdir)/src/zrtp_crypto_hash.c \
					$(top_srcdir)/src/zrtp_crypto_pk.c \
					$(top_srcdir)/src/zrtp_crypto_sas.c \
					$(top_srcdir)/src/zrtp_datatypes.c \
					$(top_srcdir)/src/zrtp_engine.c \
					$(top_srcdir)/src/zrtp_iface_scheduler.c \
					$(top_srcdir)/src/zrtp_iface_sys.c \
					$(top_srcdir)/src/zrtp_initiator.c \
					$(top_srcdir)/src/zrtp_legal.c \
					$(top_srcdir)/src/zrtp_list.c \
					$(top_srcdir)/src/zrtp_log.c \
					$(top_srcdir)/src/zrtp_pbx.c \
					$(top_srcdir)/src/zrtp_protocol.c \
					$(top_srcdir)/src/zrtp_responder.c \
					$(top_srcdir)/src/zrtp_rng.c \
					$(top_srcdir)/src/zrtp_srtp_builtin.c \
					$(top_srcdir)/src/zrtp_string.c \
					$(top_srcdir)/src/zrtp_utils.c \
					$(top_srcdir)/src/zrtp_utils_proto.c \
					\
					$(top_srcdir)/third_party/bgaes/aes_modes.c \
					$(top_srcdir)/third_party/bgaes/aescrypt.c \
					$(top_srcdir)/third_party/bgaes/aeskey.c \
					$(top_srcdir)/third_party/bgaes/aestab.c \
					$(top_srcdir)/third_party/bgaes/sha1.c \
					$(top_srcdir)/third_party/bgaes/sha2.c\
					\
					$(top_srcdir)/src/zrtp_iface_cache.c
					$(top_srcdir)/src/zrtp_engine_driven.c

check_PROGRAMS = cache_test

cache_test_CPPFLAGS = 	-I$(top_srcdir)/include \
			-I$(top_srcdir)/. \
			-I$(top_srcdir)/test \
			-I$(top_srcdir)/test/cmockery \
			-I$(top_srcdir)/third_party/bgaes \
			-I$(top_srcdir)/third_party/bnlib

cache_test_SOURCES = $(top_srcdir)/test/cmockery/cmockery.c \
					 $(top_srcdir)/test/cache_test.c
cache_test_LDADD   = libzrtp.a  $(top_srcdir)/third_party/bnlib/libbn.a -lpthread

SUBDIRS =  third_party/bnlib

if HAVE_DOXYGEN
doc: .stamp-doc
.stamp-doc:
	(cd doc && $(DOXYGEN) Doxyfile)
	touch $@
endif

uninstall:
	rm -rf $(prefix)/include/libzrtp
	rm -f $(prefix)/lib/libzrtp.a


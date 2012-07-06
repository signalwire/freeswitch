#
# Copyright (c) 2006-2009 Philip R. Zimmermann. All rights reserved.
# Contact: http://philzimmermann.com
# 
# Viktor Krikun <v.krikun@soft-industry.com> <v.krikun@gmail.com>
#

INCLUDES = 	-I$(top_srcdir)/include \
			-I$(top_srcdir)/. \
			-I$(top_srcdir)/third_party/bgaes \
			-I$(top_srcdir)/third_party/bnlib

lib_LIBRARIES = libzrtp.a

libzrtp_a_LIBADD = $(top_srcdir)/third_party/bnlib/libbn.a

libzrtp_a_SOURCES = $(top_srcdir)/src/zrtp.c \
					$(top_srcdir)/src/zrtp_crc.c \
					$(top_srcdir)/src/zrtp_crypto_aes.c \
					$(top_srcdir)/src/zrtp_crypto_atl.c \
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
if ZRTP_BUILD_ENTERPRISE
libzrtp_a_SOURCES +=$(top_srcdir)/src/zrtp_crypto_ec.c \
					$(top_srcdir)/src/zrtp_crypto_ecdh.c
endif

SUBDIRS = test

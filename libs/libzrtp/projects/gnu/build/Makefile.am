#
# Copyright (c) 2006-2009 Philip R. Zimmermann. All rights reserved.
# Contact: http://philzimmermann.com
# 
# Viktor Krikun <v.krikun@soft-industry.com> <v.krikun@gmail.com>
#

TOP_SRCDIR=$(top_srcdir)/../..
INCLUDES = 	-I$(TOP_SRCDIR)/include \
			-I$(TOP_SRCDIR)/. \
			-I$(TOP_SRCDIR)/third_party/bgaes \
			-I$(TOP_SRCDIR)/third_party/bnlib

lib_LIBRARIES = libzrtp.a

libzrtp_a_LIBADD = $(TOP_SRCDIR)/third_party/bnlib/libbn.a

libzrtp_a_SOURCES = $(TOP_SRCDIR)/src/zrtp.c \
					$(TOP_SRCDIR)/src/zrtp_crc.c \
					$(TOP_SRCDIR)/src/zrtp_crypto_aes.c \
					$(TOP_SRCDIR)/src/zrtp_crypto_atl.c \
					$(TOP_SRCDIR)/src/zrtp_crypto_hash.c \
					$(TOP_SRCDIR)/src/zrtp_crypto_pk.c \
					$(TOP_SRCDIR)/src/zrtp_crypto_sas.c \
					$(TOP_SRCDIR)/src/zrtp_datatypes.c \
					$(TOP_SRCDIR)/src/zrtp_engine.c \
					$(TOP_SRCDIR)/src/zrtp_iface_scheduler.c \
					$(TOP_SRCDIR)/src/zrtp_iface_sys.c \
					$(TOP_SRCDIR)/src/zrtp_initiator.c \
					$(TOP_SRCDIR)/src/zrtp_legal.c \
					$(TOP_SRCDIR)/src/zrtp_list.c \
					$(TOP_SRCDIR)/src/zrtp_log.c \
					$(TOP_SRCDIR)/src/zrtp_pbx.c \
					$(TOP_SRCDIR)/src/zrtp_protocol.c \
					$(TOP_SRCDIR)/src/zrtp_responder.c \
					$(TOP_SRCDIR)/src/zrtp_rng.c \
					$(TOP_SRCDIR)/src/zrtp_srtp_builtin.c \
					$(TOP_SRCDIR)/src/zrtp_string.c \
					$(TOP_SRCDIR)/src/zrtp_utils.c \
					$(TOP_SRCDIR)/src/zrtp_utils_proto.c \
					\
					$(TOP_SRCDIR)/third_party/bgaes/aes_modes.c \
					$(TOP_SRCDIR)/third_party/bgaes/aescrypt.c \
					$(TOP_SRCDIR)/third_party/bgaes/aeskey.c \
					$(TOP_SRCDIR)/third_party/bgaes/aestab.c \
					$(TOP_SRCDIR)/third_party/bgaes/sha1.c \
					$(TOP_SRCDIR)/third_party/bgaes/sha2.c\
					\
					$(TOP_SRCDIR)/src/zrtp_iface_cache.c \
					$(TOP_SRCDIR)/src/zrtp_engine_driven.c
if ZRTP_BUILD_ENTERPRISE
libzrtp_a_SOURCES +=$(TOP_SRCDIR)/src/zrtp_crypto_ec.c \
					$(TOP_SRCDIR)/src/zrtp_crypto_ecdh.c
endif

SUBDIRS = test

#
# Copyright (c) 2006-2007 Philip R. Zimmermann. All rights reserved.
# Contact: http://philzimmermann.com
# 
# Viktor Krikun <v.krikun@soft-industry.com> <v.krikun@gmail.com>
#

TEST_DIR=$(top_srcdir)/../../test
TOP_SRCDIR=$(top_srcdir)/../../include
THIRD_DIR=$(top_srcdir)/../../third_party

libzrtp_includedir=$(includedir)/libzrtp

libzrtp_include_HEADERS = \
					$(TOP_SRCDIR)/zrtp.h \
					$(TOP_SRCDIR)/zrtp_base.h \
					$(TOP_SRCDIR)/zrtp_config.h \
					$(TOP_SRCDIR)/zrtp_config_user.h \
					$(TOP_SRCDIR)/zrtp_config_unix.h \
					$(TOP_SRCDIR)/zrtp_crypto.h \
					$(TOP_SRCDIR)/zrtp_engine.h \
					$(TOP_SRCDIR)/zrtp_error.h \
					$(TOP_SRCDIR)/zrtp_iface.h \
					$(TOP_SRCDIR)/zrtp_iface_scheduler.h \
					$(TOP_SRCDIR)/zrtp_iface_cache.h \
					$(TOP_SRCDIR)/zrtp_iface_system.h \
					$(TOP_SRCDIR)/zrtp_legal.h \
					$(TOP_SRCDIR)/zrtp_list.h \
					$(TOP_SRCDIR)/zrtp_log.h \
					$(TOP_SRCDIR)/zrtp_pbx.h \
					$(TOP_SRCDIR)/zrtp_protocol.h \
					$(TOP_SRCDIR)/zrtp_srtp.h \
					$(TOP_SRCDIR)/zrtp_srtp_builtin.h \
					$(TOP_SRCDIR)/zrtp_string.h \
					$(TOP_SRCDIR)/zrtp_types.h \
					$(TOP_SRCDIR)/zrtp_version.h \
					\
					$(THIRD_DIR)/bnlib/bn.h \
					\
					$(THIRD_DIR)/bgaes/aes.h \
					$(THIRD_DIR)/bgaes/aesopt.h \
					$(THIRD_DIR)/bgaes/aestab.h \
					$(THIRD_DIR)/bgaes/bg2zrtp.h \
					$(THIRD_DIR)/bgaes/brg_types.h \
					$(THIRD_DIR)/bgaes/sha1.h \
					$(THIRD_DIR)/bgaes/sha2.h

if ZRTP_BUILD_ENTERPRISE
libzrtp_include_HEADERS += $(TOP_SRCDIR)/zrtp_ec.h
endif

SUBDIRS =  ../../third_party/bnlib
SUBDIRS += build

uninstall:
	rm -rf $(prefix)/include/libzrtp
	rm -f $(prefix)/lib/libzrtp.a

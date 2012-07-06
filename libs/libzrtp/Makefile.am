#
# Copyright (c) 2006-2007 Philip R. Zimmermann. All rights reserved.
# Contact: http://philzimmermann.com
# 
# Viktor Krikun <v.krikun@soft-industry.com> <v.krikun@gmail.com>
#

libzrtp_includedir=$(includedir)/libzrtp
libzrtp_include_HEADERS = \
					$(top_srcdir)/include/zrtp.h \
					$(top_srcdir)/include/zrtp_base.h \
					$(top_srcdir)/include/zrtp_config.h \
					$(top_srcdir)/include/zrtp_config_user.h \
					$(top_srcdir)/include/zrtp_config_unix.h \
					$(top_srcdir)/include/zrtp_crypto.h \
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

if ZRTP_BUILD_ENTERPRISE
libzrtp_include_HEADERS += $(top_srcdir)/include/zrtp_ec.h
endif

SUBDIRS =  third_party/bnlib
SUBDIRS += build

if HAVE_DOXYGEN
doc: .stamp-doc
.stamp-doc:
	(cd doc && $(DOXYGEN) Doxyfile)
	touch $@
endif

uninstall:
	rm -rf $(prefix)/include/libzrtp
	rm -f $(prefix)/lib/libzrtp.a


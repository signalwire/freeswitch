include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_enum

LDNS_DIR=$(switch_srcdir)/libs/ldns
mod_enum_la_CFLAGS = -I$(LDNS_DIR) -I$(LDNS_DIR)/include -DHAVE_POLL $(AM_CFLAGS) 

mod_LTLIBRARIES = mod_enum.la
mod_enum_la_SOURCES  = mod_enum.c

mod_enum_la_SOURCES += $(LDNS_DIR)/buffer.c\
$(LDNS_DIR)/dname.c\
$(LDNS_DIR)/dnssec.c\
$(LDNS_DIR)/dnssec_sign.c\
$(LDNS_DIR)/dnssec_verify.c\
$(LDNS_DIR)/dnssec_zone.c\
$(LDNS_DIR)/error.c\
$(LDNS_DIR)/higher.c\
$(LDNS_DIR)/host2str.c\
$(LDNS_DIR)/host2wire.c\
$(LDNS_DIR)/keys.c\
$(LDNS_DIR)/linktest.c\
$(LDNS_DIR)/net.c\
$(LDNS_DIR)/packet.c\
$(LDNS_DIR)/parse.c\
$(LDNS_DIR)/rbtree.c\
$(LDNS_DIR)/rdata.c\
$(LDNS_DIR)/resolver.c\
$(LDNS_DIR)/rr.c\
$(LDNS_DIR)/rr_functions.c\
$(LDNS_DIR)/sha1.c\
$(LDNS_DIR)/sha2.c\
$(LDNS_DIR)/str2host.c\
$(LDNS_DIR)/tsig.c\
$(LDNS_DIR)/update.c\
$(LDNS_DIR)/util.c\
$(LDNS_DIR)/wire2host.c\
$(LDNS_DIR)/compat/strlcpy.c\
$(LDNS_DIR)/compat/b64_pton.c\
$(LDNS_DIR)/compat/b32_pton.c\
$(LDNS_DIR)/compat/b64_ntop.c\
$(LDNS_DIR)/compat/b32_ntop.c\
$(LDNS_DIR)/zone.c

mod_enum_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_enum_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_enum

UDNS_DIR=$(switch_srcdir)/libs/udns
mod_enum_la_CFLAGS = -I$(UDNS_DIR) -DHAVE_POLL $(AM_CFLAGS)

mod_LTLIBRARIES = mod_enum.la
mod_enum_la_SOURCES  = mod_enum.c 
mod_enum_la_SOURCES += $(UDNS_DIR)/udns_dn.c $(UDNS_DIR)/udns_dntosp.c $(UDNS_DIR)/udns_parse.c $(UDNS_DIR)/udns_misc.c
mod_enum_la_SOURCES += $(UDNS_DIR)/udns_rr_a.c $(UDNS_DIR)/udns_rr_ptr.c $(UDNS_DIR)/udns_rr_mx.c $(UDNS_DIR)/udns_rr_txt.c $(UDNS_DIR)/udns_bl.c
mod_enum_la_SOURCES += $(UDNS_DIR)/udns_rr_srv.c $(UDNS_DIR)/udns_rr_naptr.c $(UDNS_DIR)/udns_codes.c $(UDNS_DIR)/udns_resolver.c
mod_enum_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_enum_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

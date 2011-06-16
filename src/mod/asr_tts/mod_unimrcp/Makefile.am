include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_unimrcp

UNIMRCP_DIR=$(switch_srcdir)/libs/unimrcp
UNIMRCP_BUILDDIR=$(switch_builddir)/libs/unimrcp
UNIMRCPLA=$(UNIMRCP_BUILDDIR)/platforms/libunimrcp-client/libunimrcpclient.la

mod_LTLIBRARIES = mod_unimrcp.la
mod_unimrcp_la_SOURCES = mod_unimrcp.c
mod_unimrcp_la_CFLAGS  = $(AM_CFLAGS) 
mod_unimrcp_la_CFLAGS += -I.
mod_unimrcp_la_CFLAGS += -I$(switch_srcdir)/libs/apr/include
mod_unimrcp_la_CFLAGS += -I$(switch_srcdir)/libs/apr-util/include
mod_unimrcp_la_CFLAGS += -I$(switch_builddir)/libs/apr/include
mod_unimrcp_la_CFLAGS += -I$(switch_builddir)/libs/apr-util/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/build
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/platforms/libunimrcp-client/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/modules/mrcp-sofiasip/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/modules/mrcp-unirtsp/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcp-client/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcp-signaling/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcpv2-transport/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcp/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcp/message/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcp/control/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcp/resources/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mpf/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/apr-toolkit/include

mod_unimrcp_la_LIBADD = $(switch_builddir)/libfreeswitch.la $(UNIMRCPLA) $(switch_builddir)/libs/apr/libapr-1.la $(switch_builddir)/libs/apr-util/libaprutil-1.la $(switch_builddir)/libs/apr-util/xml/expat/lib/libexpat.la
mod_unimrcp_la_LDFLAGS = -avoid-version -module -no-undefined -shared

if ISMAC
mod_unimrcp_la_LDFLAGS += -framework CoreFoundation -framework SystemConfiguration
endif

BUILT_SOURCES = $(UNIMRCPLA)

$(UNIMRCPLA): $(UNIMRCP_DIR) $(UNIMRCP_DIR)/.update
	cd $(UNIMRCP_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

$(mod_unimrcp_la_SOURCES) : $(BUILT_SOURCES)

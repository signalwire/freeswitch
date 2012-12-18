include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_sofia

SOFIA_DIR=$(switch_srcdir)/libs/sofia-sip
SOFIA_BUILDDIR=$(switch_builddir)/libs/sofia-sip
SOFIAUA_DIR=$(SOFIA_DIR)/libsofia-sip-ua
SOFIAUA_BUILDDIR=$(SOFIA_BUILDDIR)/libsofia-sip-ua
SOFIALA=$(SOFIAUA_BUILDDIR)/libsofia-sip-ua.la

mod_LTLIBRARIES = mod_sofia.la
mod_sofia_la_SOURCES = mod_sofia.c sofia.c sofia_glue.c sofia_presence.c sofia_reg.c sofia_media.c sip-dig.c rtp.c mod_sofia.h
mod_sofia_la_CFLAGS  = $(AM_CFLAGS) -I. $(SOFIA_CMD_LINE_CFLAGS)
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/bnf -I$(SOFIAUA_BUILDDIR)/bnf
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/http -I$(SOFIAUA_BUILDDIR)/http
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/iptsec -I$(SOFIAUA_BUILDDIR)/iptsec
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/nea -I$(SOFIAUA_BUILDDIR)/nea
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/nth -I$(SOFIAUA_BUILDDIR)/nth
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/sdp -I$(SOFIAUA_BUILDDIR)/sdp
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/soa -I$(SOFIAUA_BUILDDIR)/soa
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/stun -I$(SOFIAUA_BUILDDIR)/stun
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/tport -I$(SOFIAUA_BUILDDIR)/tport
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/features -I$(SOFIAUA_BUILDDIR)/features
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/ipt -I$(SOFIAUA_BUILDDIR)/ipt
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/msg -I$(SOFIAUA_BUILDDIR)/msg
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/nta -I$(SOFIAUA_BUILDDIR)/nta
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/nua -I$(SOFIAUA_BUILDDIR)/nua
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/sip -I$(SOFIAUA_BUILDDIR)/sip
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/sresolv -I$(SOFIAUA_BUILDDIR)/sresolv
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/su -I$(SOFIAUA_BUILDDIR)/su
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/url -I$(SOFIAUA_BUILDDIR)/url
mod_sofia_la_LIBADD = $(switch_builddir)/libfreeswitch.la $(SOFIALA)
mod_sofia_la_LDFLAGS = -avoid-version -module -no-undefined -shared

if ISMAC
mod_sofia_la_LDFLAGS += -framework CoreFoundation -framework SystemConfiguration
endif

BUILT_SOURCES = $(SOFIALA)

$(mod_sofia_la_SOURCES) : $(BUILT_SOURCES)

$(SOFIALA): $(SOFIA_DIR) $(SOFIA_DIR)/.update
	cd $(SOFIA_BUILDDIR) && $(MAKE) noop
	cd $(SOFIA_BUILDDIR) && $(MAKE) SOFIA_CFLAGS="$(SWITCH_AM_CFLAGS)"
	$(TOUCH_TARGET)

$(SOFIAUA_BUILDDIR)/nua/sofia-sip/nua_tag.h: $(SOFIALA)


include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_sofia

SOFIA_DIR=$(switch_builddir)/libs/sofia-sip
SOFIAUA_DIR=$(SOFIA_DIR)/libsofia-sip-ua
SOFIALA=$(SOFIAUA_DIR)/libsofia-sip-ua.la

mod_LTLIBRARIES = mod_sofia.la
mod_sofia_la_SOURCES = mod_sofia.c sofia.c sofia_glue.c sofia_presence.c sofia_reg.c sofia_sla.c  mod_sofia.h
mod_sofia_la_CFLAGS  = $(AM_CFLAGS)
mod_sofia_la_CFLAGS += -I. -I$(SOFIAUA_DIR)/bnf  -I$(SOFIAUA_DIR)/features
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/http -I$(SOFIAUA_DIR)/ipt
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/iptsec -I$(SOFIAUA_DIR)/msg
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/nea -I$(SOFIAUA_DIR)/nta
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/nth -I$(SOFIAUA_DIR)/nua
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/sdp -I$(SOFIAUA_DIR)/sip
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/soa -I$(SOFIAUA_DIR)/sresolv
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/stun -I$(SOFIAUA_DIR)/su
mod_sofia_la_CFLAGS += -I$(SOFIAUA_DIR)/tport -I$(SOFIAUA_DIR)/url
mod_sofia_la_LIBADD = $(switch_builddir)/libfreeswitch.la $(SOFIALA)
mod_sofia_la_LDFLAGS = -avoid-version -module -no-undefined

if ADD_ODBC
mod_sofia_la_CFLAGS += -DSWITCH_HAVE_ODBC
endif

if ISMAC
mod_sofia_la_LDFLAGS += -framework CoreFoundation -framework SystemConfiguration
endif

BUILT_SOURCES = $(SOFIALA)

$(mod_sofia_la_SOURCES) : $(BUILT_SOURCES)

$(SOFIALA): $(SOFIA_DIR) $(SOFIA_DIR)/.update
	cd $(SOFIA_DIR) && $(MAKE) SOFIA_CFLAGS="$(SWITCH_AM_CFLAGS)"
	$(TOUCH_TARGET)

../../../../libs/sofia-sip/libsofia-sip-ua/nua/sofia-sip/nua_tag.h: $(SOFIALA)


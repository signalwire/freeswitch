include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_sofia

SOFIA_DIR=$(switch_srcdir)/libs/sofia-sip
SOFIA_BUILDDIR=$(switch_builddir)/libs/sofia-sip
SOFIAUA_DIR=$(SOFIA_DIR)/libsofia-sip-ua
SOFIAUA_BUILDDIR=$(SOFIA_BUILDDIR)/libsofia-sip-ua
SOFIALA=$(SOFIAUA_BUILDDIR)/libsofia-sip-ua.la

noinst_LTLIBRARIES = libsofiamod.la
libsofiamod_la_SOURCES   =  mod_sofia.c sofia.c sofia_json_api.c sofia_glue.c sofia_presence.c sofia_reg.c sofia_media.c sip-dig.c rtp.c mod_sofia.h
#libsofiamod_la_LIBADD = $(switch_builddir)/libfreeswitch.la $(SOFIALA)
libsofiamod_la_LDFLAGS   = -static
libsofiamod_la_CFLAGS  = $(AM_CFLAGS) -I. $(SOFIA_CMD_LINE_CFLAGS)
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/bnf -I$(SOFIAUA_BUILDDIR)/bnf
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/http -I$(SOFIAUA_BUILDDIR)/http
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/iptsec -I$(SOFIAUA_BUILDDIR)/iptsec
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/nea -I$(SOFIAUA_BUILDDIR)/nea
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/nth -I$(SOFIAUA_BUILDDIR)/nth
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/sdp -I$(SOFIAUA_BUILDDIR)/sdp
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/soa -I$(SOFIAUA_BUILDDIR)/soa
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/stun -I$(SOFIAUA_BUILDDIR)/stun
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/tport -I$(SOFIAUA_BUILDDIR)/tport
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/features -I$(SOFIAUA_BUILDDIR)/features
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/ipt -I$(SOFIAUA_BUILDDIR)/ipt
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/msg -I$(SOFIAUA_BUILDDIR)/msg
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/nta -I$(SOFIAUA_BUILDDIR)/nta
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/nua -I$(SOFIAUA_BUILDDIR)/nua
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/sip -I$(SOFIAUA_BUILDDIR)/sip
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/sresolv -I$(SOFIAUA_BUILDDIR)/sresolv
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/su -I$(SOFIAUA_BUILDDIR)/su
libsofiamod_la_CFLAGS += -I$(SOFIAUA_DIR)/url -I$(SOFIAUA_BUILDDIR)/url

mod_LTLIBRARIES = mod_sofia.la
mod_sofia_la_SOURCES =
mod_sofia_la_LIBADD = $(switch_builddir)/libfreeswitch.la $(SOFIALA) libsofiamod.la
mod_sofia_la_LDFLAGS = -avoid-version -module -no-undefined -shared

BUILT_SOURCES = $(SOFIALA)

noinst_PROGRAMS = test/test_sofia_funcs

test_test_sofia_funcs_SOURCES = test/test_sofia_funcs.c
test_test_sofia_funcs_CFLAGS = $(AM_CFLAGS) -I $(libsofiamod_la_CFLAGS) 
test_test_sofia_funcs_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS) $(SOFIALA)
test_test_sofia_funcs_LDADD = libsofiamod.la $(SOFIALA)
TESTS = $(noinst_PROGRAMS)

if ISMAC
mod_sofia_la_LDFLAGS += -framework CoreFoundation -framework SystemConfiguration
test_test_sofia_funcs_LDFLAGS += -framework CoreFoundation -framework SystemConfiguration
endif

$(mod_sofia_la_SOURCES) : $(BUILT_SOURCES)

$(SOFIALA): $(SOFIA_DIR) $(SOFIA_DIR)/.update
	cd $(SOFIA_BUILDDIR) && $(MAKE) noop
	cd $(SOFIA_BUILDDIR) && $(MAKE) SOFIA_CFLAGS="$(SWITCH_AM_CFLAGS)"
	$(TOUCH_TARGET)

$(SOFIAUA_BUILDDIR)/nua/sofia-sip/nua_tag.h: $(SOFIALA)


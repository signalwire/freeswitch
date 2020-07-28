include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_sofia

noinst_LTLIBRARIES = libsofiamod.la
libsofiamod_la_SOURCES   =  mod_sofia.c sofia.c sofia_json_api.c sofia_glue.c sofia_presence.c sofia_reg.c sofia_media.c sip-dig.c rtp.c mod_sofia.h
libsofiamod_la_LDFLAGS   = -static
libsofiamod_la_CFLAGS  = $(AM_CFLAGS) -I. $(SOFIA_SIP_CFLAGS)

mod_LTLIBRARIES = mod_sofia.la
mod_sofia_la_SOURCES =
mod_sofia_la_LIBADD = $(switch_builddir)/libfreeswitch.la libsofiamod.la
mod_sofia_la_LDFLAGS = -avoid-version -module -no-undefined -shared $(SOFIA_SIP_LIBS)

noinst_PROGRAMS = test/test_sofia_funcs

test_test_sofia_funcs_SOURCES = test/test_sofia_funcs.c
test_test_sofia_funcs_CFLAGS = $(AM_CFLAGS) $(SOFIA_SIP_CFLAGS) -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\"
test_test_sofia_funcs_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS) 
test_test_sofia_funcs_LDADD = libsofiamod.la $(SOFIA_SIP_LIBS)

TESTS = $(noinst_PROGRAMS)

if ISMAC
mod_sofia_la_LDFLAGS += -framework CoreFoundation -framework SystemConfiguration
test_test_sofia_funcs_LDFLAGS += -framework CoreFoundation -framework SystemConfiguration
endif


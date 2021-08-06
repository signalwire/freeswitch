include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_sofia

noinst_LTLIBRARIES = libsofiamod.la
libsofiamod_la_SOURCES   =  mod_sofia.c sofia.c sofia_json_api.c sofia_glue.c sofia_presence.c sofia_reg.c sofia_media.c sip-dig.c rtp.c mod_sofia.h
#libsofiamod_la_LIBADD = $(switch_builddir)/libfreeswitch.la $(SOFIALA)
libsofiamod_la_LDFLAGS   =  -lstdc++ -static
libsofiamod_la_CFLAGS  = $(AM_CFLAGS) -I. $(SOFIA_CMD_LINE_CFLAGS) $(SOFIA_SIP_CFLAGS)

mod_LTLIBRARIES = mod_sofia.la

mod_sofia_la_SOURCES = prometheus_metrics.cpp prometheus_metrics.h
mod_sofia_la_LIBADD = $(switch_builddir)/libfreeswitch.la $(SOFIALA) libsofiamod.la
mod_sofia_la_LDFLAGS = -avoid-version -module -no-undefined -shared  -lstdc++ $(SOFIA_SIP_LIBS)

BUILT_SOURCES = $(SOFIALA)


#noinst_PROGRAMS = test/test_sofia_funcs

#test_test_sofia_funcs_SOURCES = test/test_sofia_funcs.c prometheus_metrics.cpp prometheus_metrics.h
#test_test_sofia_funcs_CFLAGS = $(AM_CFLAGS) -I $(libsofiamod_la_CFLAGS) 
#test_test_sofia_funcs_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS) $(SOFIALA) -lstdc++
#test_test_sofia_funcs_LDADD = libsofiamod.la $(SOFIALA) -lstdc++
#TESTS = $(noinst_PROGRAMS)

#if ISMAC
#mod_sofia_la_LDFLAGS += -framework CoreFoundation -framework SystemConfiguration
#test_test_sofia_funcs_LDFLAGS += -framework CoreFoundation -framework SystemConfiguration
#endif


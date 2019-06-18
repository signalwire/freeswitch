include $(top_srcdir)/build/modmake.rulesam

bin_PROGRAMS = switch_event switch_hash switch_ivr_originate switch_utils switch_core switch_console
AM_LDFLAGS  = -avoid-version -no-undefined $(SWITCH_AM_LDFLAGS) $(openssl_LIBS)
AM_LDFLAGS += $(FREESWITCH_LIBS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
AM_CFLAGS   = $(SWITCH_AM_CPPFLAGS)
AM_CPPFLAGS = $(SWITCH_AM_CPPFLAGS)

TESTS = $(bin_PROGRAMS)

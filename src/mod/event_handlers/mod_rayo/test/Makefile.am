include $(top_srcdir)/build/modmake.rulesam
noinst_PROGRAMS = test_iks test_nlsml test_srgs

test_iks_CFLAGS = $(AM_CFLAGS) -I../ -I$(switch_builddir)/libs/iksemel/include $(PCRE_CFLAGS)
test_iks_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) ../mod_rayo.la $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)

test_nlsml_CFLAGS = $(AM_CFLAGS) -I../ -I$(switch_builddir)/libs/iksemel/include $(PCRE_CFLAGS)
test_nlsml_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) ../mod_rayo.la $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)

test_srgs_CFLAGS = $(AM_CFLAGS) -I../ -I$(switch_builddir)/libs/iksemel/include $(PCRE_CFLAGS)
test_srgs_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) ../mod_rayo.la $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)

TESTS = $(noinst_PROGRAMS)

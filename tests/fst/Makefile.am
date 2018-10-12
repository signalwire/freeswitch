include $(top_srcdir)/build/modmake.rulesam

bin_PROGRAMS = originate_test
AM_LDFLAGS  = -avoid-version -no-undefined $(SWITCH_AM_LDFLAGS) $(openssl_LIBS) $(FREESWITCH_LIBS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
AM_CFLAGS   = $(SWITCH_AM_CPPFLAGS)
AM_CPPFLAGS = $(SWITCH_AM_CPPFLAGS)

TESTS = $(bin_PROGRAMS)

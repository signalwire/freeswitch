include $(top_srcdir)/build/modmake.rulesam
noinst_PROGRAMS = test_asr
test_asr_CFLAGS = $(AM_CFLAGS) -I../
test_asr_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
test_asr_LDADD = ../libtestmod.la
TESTS = $(noinst_PROGRAMS)

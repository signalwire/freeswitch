include $(top_srcdir)/build/modmake.rulesam

undefine AM_CFLAGS

noinst_PROGRAMS = switch_event switch_hash switch_ivr_originate switch_utils switch_core switch_console switch_vpx switch_core_file \
			   switch_ivr_play_say switch_core_codec switch_rtp switch_xml
noinst_PROGRAMS+= switch_core_video switch_core_db
AM_LDFLAGS += -avoid-version -no-undefined $(openssl_LIBS)
AM_LDFLAGS += $(FREESWITCH_LIBS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)

TESTS = $(noinst_PROGRAMS)

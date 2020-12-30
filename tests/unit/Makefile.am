include $(top_srcdir)/build/modmake.rulesam

noinst_PROGRAMS = switch_event switch_hash switch_ivr_originate switch_utils switch_core switch_console switch_vpx switch_core_file \
			   switch_ivr_play_say switch_core_codec switch_rtp switch_xml
noinst_PROGRAMS+= switch_core_video switch_core_db switch_vad switch_core_asr
AM_LDFLAGS  = -avoid-version -no-undefined $(SWITCH_AM_LDFLAGS) $(openssl_LIBS)
AM_LDFLAGS += $(FREESWITCH_LIBS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
AM_CFLAGS   = $(SWITCH_AM_CPPFLAGS)
AM_CPPFLAGS = $(SWITCH_AM_CPPFLAGS)

if HAVE_FVAD
AM_CFLAGS += -DSWITCH_HAVE_FVAD
endif

TESTS = $(noinst_PROGRAMS)

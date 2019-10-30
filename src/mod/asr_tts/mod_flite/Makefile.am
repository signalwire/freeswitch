include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_flite

FLITE_V2_DIR=$(switch_srcdir)/libs/flite
FLITE_V2_BUILDDIR=$(switch_builddir)/libs/flite
FLITE_V2_LIBDIR=$(FLITE_V2_BUILDDIR)/build/x86_64-linux-gnu/lib
FLITE_V2_LA=$(FLITE_V2_LIBDIR)/libflite.a

FLITE_V2_LIBS = $(FLITE_V2_LIBDIR)/libflite_cmu_time_awb.a $(FLITE_V2_LIBDIR)/libflite_cmu_us_awb.a \
		$(FLITE_V2_LIBDIR)/libflite_cmu_us_kal16.a $(FLITE_V2_LIBDIR)/libflite_cmu_us_kal.a \
		$(FLITE_V2_LIBDIR)/libflite_cmu_us_rms.a $(FLITE_V2_LIBDIR)/libflite_cmu_us_slt.a \
		$(FLITE_V2_LIBDIR)/libflite_usenglish.a $(FLITE_V2_LIBDIR)/libflite_cmu_grapheme_lang.a \
		$(FLITE_V2_LIBDIR)/libflite_cmulex.a $(FLITE_V2_LIBDIR)/libflite_cmu_grapheme_lex.a \
		$(FLITE_V2_LIBDIR)/libflite_cmu_indic_lex.a $(FLITE_V2_LIBDIR)/libflite_cmu_indic_lang.a \
		$(FLITE_V2_LIBDIR)/libflite.a

mod_LTLIBRARIES = mod_flite.la
mod_flite_la_SOURCES  = mod_flite.c
mod_flite_la_CFLAGS   = $(AM_CFLAGS) $(FLITE_CFLAGS)
mod_flite_la_CFLAGS  += -I$(FLITE_V2_DIR)/include
mod_flite_la_LIBADD   = $(FLITE_V2_LIBS) $(switch_builddir)/libfreeswitch.la
mod_flite_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES = $(FLITE_V2_LA)

$(FLITE_V2_LA):
	@cd $(FLITE_V2_BUILDDIR) && $(MAKE)
include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_flite

FLITE=flite-2.0.0

FLITE_DIR=$(switch_srcdir)/libs/$(FLITE)-release
FLITE_BUILDDIR=$(switch_builddir)/libs/$(FLITE)-release

FLITE_LIBDIR=$(FLITE_BUILDDIR)/build/libs
FLITE_A=$(FLITE_LIBDIR)/libflite_cmu_us_awb.a \
	$(FLITE_LIBDIR)/libflite_cmu_us_kal.a \
	$(FLITE_LIBDIR)/libflite_cmu_us_kal16.a \
	$(FLITE_LIBDIR)/libflite_cmu_us_rms.a \
	$(FLITE_LIBDIR)/libflite_cmu_us_slt.a \
	$(FLITE_LIBDIR)/libflite_cmulex.a $(FLITE_LIBDIR)/libflite_usenglish.a $(FLITE_LIBDIR)/libflite.a -lm

mod_LTLIBRARIES = mod_flite.la
mod_flite_la_SOURCES  = mod_flite.c
mod_flite_la_CFLAGS   = $(AM_CFLAGS) -I$(FLITE_DIR)/include -I$(FLITE_BUILDDIR)/include
mod_flite_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(FLITE_A)
mod_flite_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES= $(FLITE_A)

$(FLITE_DIR):
	$(GETLIB) $(FLITE)-release.tar.bz2

$(FLITE_BUILDDIR)/.stamp-configure: $(FLITE_DIR)
	mkdir -p $(FLITE_BUILDDIR)
	cd $(FLITE_BUILDDIR) && $(DEFAULT_VARS) $(FLITE_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(FLITE_DIR) --with-audio=none --with-pic --disable-shared
	test -f Makefile && touch $@

$(FLITE_A): $(FLITE_DIR) $(FLITE_BUILDDIR)/.stamp-configure
	cd $(FLITE_BUILDDIR) && $(MAKE) -j1
	test -d $(FLITE_LIBDIR) || mkdir $(FLITE_LIBDIR)
	TARGET_OS=`grep TARGET_OS $(FLITE_BUILDDIR)/config/config | sed "s/^.*= //"` ;\
	TARGET_CPU=`grep TARGET_CPU $(FLITE_BUILDDIR)/config/config | sed "s/^.*= //"` ;\
	cd $(FLITE_BUILDDIR) && cp build/$$TARGET_CPU-$$TARGET_OS/lib/*.a $(FLITE_LIBDIR)

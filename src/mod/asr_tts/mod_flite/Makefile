FLITE=flite-1.5.4

FLITE_DIR=$(switch_srcdir)/libs/$(FLITE)-current
FLITE_BUILDDIR=$(switch_builddir)/libs/$(FLITE)-current

FLITE_LIBDIR=$(FLITE_BUILDDIR)/build/libs
FLITE_A=$(FLITE_LIBDIR)/libflite_cmu_us_awb.a \
	$(FLITE_LIBDIR)/libflite_cmu_us_kal.a \
	$(FLITE_LIBDIR)/libflite_cmu_us_kal16.a \
	$(FLITE_LIBDIR)/libflite_cmu_us_rms.a \
	$(FLITE_LIBDIR)/libflite_cmu_us_slt.a \
	$(FLITE_LIBDIR)/libflite_cmulex.a $(FLITE_LIBDIR)/libflite_usenglish.a $(FLITE_LIBDIR)/libflite.a -lm

LOCAL_CFLAGS=-I$(FLITE_DIR)/include -I$(FLITE_BUILDDIR)/include
LOCAL_LIBADD=$(FLITE_A)

BASE=../../../..
include $(BASE)/build/modmake.rules

$(MODNAME).lo: $(FLITE_A)
$(FLITE_DIR):
	$(GETLIB) $(FLITE)-current.tar.bz2

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

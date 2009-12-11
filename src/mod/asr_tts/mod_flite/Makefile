FLITE=flite-1.3.99

FLITE_DIR=$(switch_srcdir)/libs/$(FLITE)
FLITE_BUILDDIR=$(switch_builddir)/libs/$(FLITE)

FLITE_LIBDIR=$(FLITE_BUILDDIR)/build/libs
FLITE_A=$(FLITE_LIBDIR)/libflite_cmu_us_awb.a \
	$(FLITE_LIBDIR)/libflite_cmu_us_kal.a \
	$(FLITE_LIBDIR)/libflite_cmu_us_rms.a \
	$(FLITE_LIBDIR)/libflite_cmu_us_slt.a \
	$(FLITE_LIBDIR)/libflite_cmulex.a $(FLITE_LIBDIR)/libflite_usenglish.a $(FLITE_LIBDIR)/libflite.a -lm

LOCAL_CFLAGS=-I$(FLITE_DIR)/include -I$(FLITE_BUILDDIR)/include
LOCAL_LIBADD=$(FLITE_A)

BASE=../../../..
include $(BASE)/build/modmake.rules

$(FLITE_DIR):
	$(GETLIB) $(FLITE)-latest.tar.gz

$(FLITE_BUILDDIR)/Makefile: $(FLITE_DIR)
	mkdir -p $(FLITE_BUILDDIR)
	cd $(FLITE_BUILDDIR) && $(DEFAULT_VARS) $(FLITE_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(FLITE_DIR) --without-audio --with-pic --disable-shared
	$(TOUCH_TARGET)

$(FLITE_A): $(FLITE_DIR) $(FLITE_BUILDDIR)/Makefile
	cd $(FLITE_BUILDDIR) && $(MAKE)
	test -d $(FLITE_LIBDIR) || mkdir $(FLITE_LIBDIR)
	cd $(FLITE_BUILDDIR) && FLITE_TMP_LIBDIR=`$(FLITE_DIR)/config.guess | sed "s/-[^-]*//" | sed "s/i[3456]86/i386/"` && cp build/$$FLITE_TMP_LIBDIR/lib/*.a $(FLITE_LIBDIR)


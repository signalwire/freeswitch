FLITE=flite-1.3-current
FLITE_DIR=$(switch_srcdir)/libs/$(FLITE)
FLITE_LIBDIR=$(FLITE_DIR)/build/libs
FLITE_A=$(FLITE_LIBDIR)/libflite_cmu_us_kal.a $(FLITE_LIBDIR)/libflite_cmu_us_kal16.a $(FLITE_LIBDIR)/libflite_cmulex.a $(FLITE_LIBDIR)/libflite_usenglish.a $(FLITE_LIBDIR)/libflite.a

LOCAL_CFLAGS=-I$(switch_srcdir)/libs/$(FLITE)/include
LOCAL_LIBADD=$(FLITE_A) -lm

BASE=../../../..
include $(BASE)/build/modmake.rules

$(FLITE_DIR):
	$(GETLIB) $(FLITE).tar.gz
	cp flite-1.3.diff $(FLITE_DIR)
	cd $(FLITE_DIR) && patch -p1 -i flite-1.3.diff 

$(FLITE_DIR)/Makefile: $(FLITE_DIR)
	cd $(FLITE_DIR) && CFLAGS="-fPIC" ./configure
	$(TOUCH_TARGET)

$(FLITE_A): $(FLITE_DIR) $(FLITE_DIR)/Makefile
	cd $(FLITE_DIR) && $(MAKE)
	test -d $(FLITE_LIBDIR) || mkdir $(FLITE_LIBDIR)
	cd $(FLITE_DIR) && FLITE_TMP_LIBDIR=`./config.guess | sed "s/-[^-]*//" | sed "s/i[3456]86/i386/"` && cp build/$$FLITE_TMP_LIBDIR/lib/*.a $(FLITE_LIBDIR)


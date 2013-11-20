BASE=../../../..
VERSION=soundtouch-1.7.1
SNDT_SRC=$(switch_srcdir)/libs/soundtouch
SNDT_BUILD=$(switch_builddir)/libs/soundtouch
LA=$(SNDT_BUILD)/source/SoundTouch/.libs/libSoundTouch.a

LOCAL_CFLAGS += -I$(SNDT_SRC)/include -DINTEGER_SAMPLES -I$(SNDT_BUILD)/include
LOCAL_LIBADD=$(LA)

include $(BASE)/build/modmake.rules

$(SNDT_SRC):
	$(GETLIB) $(VERSION).tar.gz

$(SNDT_BUILD)/Makefile: $(SNDT_SRC)
	mkdir -p $(SNDT_BUILD)/config/m4
	rm -rf $(SNDT_BUILD)/config/m4/*
	rm -rf $(SNDT_SRC)/config/m4/*
	sed -e 's:AM_CONFIG_HEADER:AC_CONFIG_HEADERS:' $(SNDT_SRC)/configure.ac > $(SNDT_SRC)/configure.ac.tmp && \
		mv $(SNDT_SRC)/configure.ac.tmp $(SNDT_SRC)/configure.ac
	cd $(SNDT_SRC) && AUTOMAKE="automake --add-missing --foreign --copy" autoreconf -fisv 
	cd ${SNDT_BUILD} && $(DEFAULT_VARS) $(SNDT_SRC)/configure $(DEFAULT_ARGS) --enable-integer-samples --srcdir=$(SNDT_SRC)
	${TOUCH_TARGET}
	

$(LA): $(SNDT_BUILD)/Makefile 
	cd $(SNDT_BUILD) && ${MAKE}
	$(TOUCH_TARGET)

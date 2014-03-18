include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_soundtouch
VERSION=soundtouch-1.7.1
SNDT_SRC=$(switch_srcdir)/libs/soundtouch
SNDT_BUILD=$(switch_builddir)/libs/soundtouch
LA=$(SNDT_BUILD)/source/SoundTouch/libSoundTouch.la

mod_LTLIBRARIES = mod_soundtouch.la
mod_soundtouch_la_SOURCES  = mod_soundtouch.cpp
mod_soundtouch_la_CFLAGS   = $(AM_CFLAGS) -I$(SNDT_SRC)/include -DINTEGER_SAMPLES -I$(SNDT_BUILD)/include
mod_soundtouch_la_CXXFLAGS = -I$(SNDT_SRC)/include -DINTEGER_SAMPLES -I$(SNDT_BUILD)/include
mod_soundtouch_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(LA)
mod_soundtouch_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(LA)

$(SNDT_SRC):
	$(GETLIB) $(VERSION).tar.gz
	sed -i'' -e 's/-fcheck-new//' $(top_srcdir)/libs/soundtouch/source/SoundTouch/Makefile.am

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

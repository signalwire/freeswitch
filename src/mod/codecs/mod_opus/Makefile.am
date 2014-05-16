include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_opus
OPUS=opus-1.1-p2
OPUS_DIR=$(switch_srcdir)/libs/$(OPUS)
OPUS_BUILDDIR=$(switch_builddir)/libs/$(OPUS)
OPUS_LA=$(OPUS_BUILDDIR)/.libs/libopus.la

mod_LTLIBRARIES = mod_opus.la
mod_opus_la_SOURCES  = mod_opus.c
mod_opus_la_CFLAGS   = $(AM_CFLAGS) -I$(OPUS_DIR)/include
mod_opus_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(OPUS_LA)
mod_opus_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lm -lz

BUILT_SOURCES=$(OPUS_LA)
$(OPUS_DIR)/configure:
	$(GETLIB) $(OPUS).tar.gz

$(OPUS_BUILDDIR)/Makefile: $(OPUS_DIR)/configure
	mkdir -p $(OPUS_BUILDDIR)
	cd $(OPUS_BUILDDIR) && $(DEFAULT_VARS) $(OPUS_DIR)/configure --disable-shared --with-pic --srcdir=$(OPUS_DIR)
	$(TOUCH_TARGET)

$(OPUS_LA): $(OPUS_BUILDDIR)/Makefile
	cd $(OPUS_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

$(MODNAME).lo: $(OPUS_LA)


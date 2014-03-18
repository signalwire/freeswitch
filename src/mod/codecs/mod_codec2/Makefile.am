include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_codec2

CODEC2_DIR=$(switch_srcdir)/libs/libcodec2
CODEC2_BUILDDIR=$(switch_builddir)/libs/libcodec2
CODEC2LA=$(CODEC2_BUILDDIR)/src/libcodec2.la

mod_LTLIBRARIES = mod_codec2.la
mod_codec2_la_SOURCES  = mod_codec2.c
mod_codec2_la_CFLAGS   = $(AM_CFLAGS) -I$(CODEC2_DIR)/include -I$(CODEC2_BUILDDIR)/src -I$(CODEC2_DIR)/src
mod_codec2_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(CODEC2LA)
mod_codec2_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(CODEC2LA)

$(CODEC2LA): $(CODEC2_DIR) $(CODEC2_DIR)/.update
	cd $(CODEC2_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)


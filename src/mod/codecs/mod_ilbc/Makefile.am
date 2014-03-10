include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_ilbc

ILBC_DIR=$(switch_srcdir)/libs/ilbc
ILBC_BUILDDIR=$(switch_builddir)/libs/ilbc
ILBCLA=$(ILBC_BUILDDIR)/src/libilbc.la

mod_LTLIBRARIES = mod_ilbc.la
mod_ilbc_la_SOURCES  = mod_ilbc.c
mod_ilbc_la_CFLAGS   = $(AM_CFLAGS) -I$(ILBC_DIR)/src -I$(ILBC_BUILDDIR)/src
mod_ilbc_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(ILBCLA)
mod_ilbc_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(ILBCLA)

$(ILBCLA): $(ILBC_DIR) $(ILBC_DIR)/.update
	cd $(ILBC_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

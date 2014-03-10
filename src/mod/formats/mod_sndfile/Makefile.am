include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_sndfile

SNDFILE_DIR=$(switch_srcdir)/libs/libsndfile
SNDFILE_BUILDDIR=$(switch_builddir)/libs/libsndfile
SNDFILELA=$(SNDFILE_BUILDDIR)/src/libsndfile.la

mod_LTLIBRARIES = mod_sndfile.la
mod_sndfile_la_SOURCES  = mod_sndfile.c
mod_sndfile_la_CFLAGS   = $(AM_CFLAGS) -I$(SNDFILE_DIR)/src -I$(SNDFILE_BUILDDIR)/src
mod_sndfile_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(SNDFILELA)
mod_sndfile_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(SNDFILELA)

$(SNDFILELA): $(SNDFILE_DIR) $(SNDFILE_DIR)/.update
	cd $(SNDFILE_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

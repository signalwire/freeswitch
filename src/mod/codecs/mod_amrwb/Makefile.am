include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_amrwb

AMRWB_DIR=$(switch_srcdir)/libs/amrwb
AMRWB_BUILDDIR=$(switch_builddir)/libs/amrwb
AMRWB_A=$(AMRWB_BUILDDIR)/libamrwb.a

mod_LTLIBRARIES = mod_amrwb.la
mod_amrwb_la_SOURCES  = mod_amrwb.c
mod_amrwb_la_CFLAGS   = $(AM_CFLAGS)
mod_amrwb_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_amrwb_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

if HAVE_AMRWB
BUILT_SOURCES= $(AMRWB_A)
mod_amrwb_la_CFLAGS += -I$(AMRWB_DIR)
mod_amrwb_la_LIBADD += $(AMRWB_A)
else
mod_amrwb_la_CFLAGS += -DAMRWB_PASSTHROUGH
endif


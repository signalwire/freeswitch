include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_amr

AMR_DIR=$(switch_srcdir)/libs/amr
AMR_BUILDDIR=$(switch_builddir)/libs/amr
AMR_A=$(AMR_BUILDDIR)/libamr.a

mod_LTLIBRARIES = mod_amr.la
mod_amr_la_SOURCES  = mod_amr.c
mod_amr_la_CFLAGS   = $(AM_CFLAGS)
mod_amr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_amr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

if HAVE_AMR
BUILT_SOURCES= $(AMR_A)
mod_amr_la_CFLAGS += -I$(AMR_DIR)
mod_amr_la_LIBADD += $(AMR_A)
else
mod_amr_la_CFLAGS += -DAMR_PASSTHROUGH
endif


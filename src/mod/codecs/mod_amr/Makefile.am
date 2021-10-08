include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_amr

mod_LTLIBRARIES = mod_amr.la
if HAVE_AMR
mod_amr_la_SOURCES  = mod_amr.c bitshift.c amr_be.c
else
mod_amr_la_SOURCES  = mod_amr.c 
endif

mod_amr_la_CFLAGS   = $(AM_CFLAGS)
mod_amr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_amr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

if HAVE_AMR
# install AMR library opencore-amrnb (check README)
mod_amr_la_CFLAGS += $(AMR_CFLAGS) 
mod_amr_la_LIBADD += $(AMR_LIBS) 
else
mod_amr_la_CFLAGS += -DAMR_PASSTHROUGH
endif


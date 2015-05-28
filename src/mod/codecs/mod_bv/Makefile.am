include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_bv

if HAVE_BROADVOICE

mod_LTLIBRARIES = mod_bv.la
mod_bv_la_SOURCES  = mod_bv.c
mod_bv_la_CFLAGS   = $(AM_CFLAGS) $(BROADVOICE_CFLAGS)
mod_bv_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(BROADVOICE_LIBS)
mod_bv_la_LDFLAGS  = -avoid-version -module -no-undefined -shared


else
install: error
all: error
error:
	$(error You must install libbroadvoice-dev to build mod_bv)
endif

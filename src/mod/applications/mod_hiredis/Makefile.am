include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_hiredis

if HAVE_HIREDIS

mod_LTLIBRARIES = mod_hiredis.la
mod_hiredis_la_SOURCES  = mod_hiredis.c hiredis_utils.c hiredis_profile.c
mod_hiredis_la_CFLAGS   = $(AM_CFLAGS) $(HIREDIS_CFLAGS)
mod_hiredis_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_hiredis_la_LDFLAGS  = -avoid-version -module -no-undefined -shared $(HIREDIS_LIBS) $(SWITCH_AM_LDFLAGS)

else
install: error
all: error
error:
	$(error You must install libhiredis-dev to build this module)
endif

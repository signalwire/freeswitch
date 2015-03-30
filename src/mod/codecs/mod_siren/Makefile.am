include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_siren

if HAVE_G7221

mod_LTLIBRARIES = mod_siren.la
mod_siren_la_SOURCES  = mod_siren.c
mod_siren_la_CFLAGS   = $(AM_CFLAGS) $(G7221_CFLAGS)
mod_siren_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(G7221_LIBS)
mod_siren_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

else
install: error
all: error
error:
	$(error You must install libg7221-dev to build mod_siren)
endif

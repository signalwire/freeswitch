include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_enum

if HAVE_LDNS
mod_LTLIBRARIES = mod_enum.la
mod_enum_la_SOURCES  = mod_enum.c
mod_enum_la_CFLAGS   = $(AM_CFLAGS)
mod_enum_la_CPPFLAGS = $(LDNS_CFLAGS) $(AM_CPPFLAGS)
mod_enum_la_LIBADD   = $(LDNS_LIBS) $(switch_builddir)/libfreeswitch.la
mod_enum_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
else
install: error
all: error
error:
	$(error You must install libldns-dev to build mod_enum)
endif

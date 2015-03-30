include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_ilbc

if HAVE_ILBC

mod_LTLIBRARIES = mod_ilbc.la
mod_ilbc_la_SOURCES  = mod_ilbc.c
mod_ilbc_la_CFLAGS   = $(AM_CFLAGS) $(ILBC_CFLAGS)
mod_ilbc_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(ILBC_LIBS)
mod_ilbc_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

else
install: error
all: error
error:
	$(error You must install libilbc-dev to build mod_ilbc)
endif

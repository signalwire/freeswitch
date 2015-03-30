include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_silk

if HAVE_SILK

mod_LTLIBRARIES = mod_silk.la
mod_silk_la_SOURCES  = mod_silk.c
mod_silk_la_CFLAGS   = $(AM_CFLAGS) $(SILK_CFLAGS)
mod_silk_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(SILK_LIBS)
mod_silk_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

else
install: error
all: error
error:
	$(error You must install libsilk-dev to build mod_silk)
endif

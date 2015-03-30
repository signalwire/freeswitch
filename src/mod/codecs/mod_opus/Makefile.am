include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_opus

if HAVE_OPUS

mod_LTLIBRARIES = mod_opus.la
mod_opus_la_SOURCES  = mod_opus.c
mod_opus_la_CFLAGS   = $(AM_CFLAGS) $(OPUS_CFLAGS)
mod_opus_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(OPUS_LIBS)
mod_opus_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lm -lz

else
install: error
all: error
error:
	$(error You must install libopus-dev to build mod_opus)
endif

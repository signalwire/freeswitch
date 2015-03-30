include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_codec2

if HAVE_CODEC2

mod_LTLIBRARIES = mod_codec2.la
mod_codec2_la_SOURCES  = mod_codec2.c
mod_codec2_la_CFLAGS   = $(AM_CFLAGS) $(CODEC2_CFLAGS)
mod_codec2_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(CODEC2_LIBS)
mod_codec2_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

else
install: error
all: error
error:
	$(error You must install libcodec2-dev to build mod_codec2)
endif

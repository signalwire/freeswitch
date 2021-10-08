include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_sndfile

if HAVE_SNDFILE

mod_LTLIBRARIES = mod_sndfile.la
mod_sndfile_la_SOURCES  = mod_sndfile.c
mod_sndfile_la_CFLAGS   = $(AM_CFLAGS) $(SNDFILE_CFLAGS)
mod_sndfile_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(SNDFILE_LIBS)
mod_sndfile_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

else
install: error
all: error
error:
	$(error You must install libsndfile-dev to build mod_sndfile)
endif

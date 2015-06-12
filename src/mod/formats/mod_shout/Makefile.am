include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_shout

if HAVE_SHOUT
if HAVE_MPG123

mod_LTLIBRARIES = mod_shout.la
mod_shout_la_SOURCES  = mod_shout.c
mod_shout_la_CFLAGS   = $(AM_CFLAGS)
mod_shout_la_CPPFLAGS = $(CURL_CFLAGS) $(AM_CPPFLAGS) $(SHOUT_CFLAGS) $(MP3LAME_CFLAGS) $(MPG123_CFLAGS)
mod_shout_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_shout_la_LDFLAGS  = $(CURL_LIBS) -avoid-version -module -no-undefined -shared $(SHOUT_LIBS) $(MP3LAME_LIBS) $(MPG123_LIBS)


if !HAVE_MP3LAME
mod_shout_la_LDFLAGS += -lmp3lame
mod_shout_la_CFLAGS += -I/usr/include/lame
endif

else
install: error
all: error
error:
	$(error You must install libmpg123-dev to build mod_shout)
endif

else
install: error
all: error
error:
	$(error You must install libshout3-dev to build mod_shout)
endif


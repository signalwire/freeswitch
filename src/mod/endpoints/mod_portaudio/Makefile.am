include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_portaudio

if HAVE_PORTAUDIO
mod_LTLIBRARIES = mod_portaudio.la
mod_portaudio_la_SOURCES = mod_portaudio.c pablio.c pa_ringbuffer.c
mod_portaudio_la_CFLAGS = $(AM_CFLAGS)
mod_portaudio_la_CPPFLAGS = -I. $(PORTAUDIO_CFLAGS) $(AM_CPPFLAGS)
mod_portaudio_la_LIBADD = $(PORTAUDIO_LIBS) $(switch_builddir)/libfreeswitch.la
mod_portaudio_la_LDFLAGS = -avoid-version -module -no-undefined -shared
if ISMAC
mod_portaudio_la_LDFLAGS += -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework Carbon
endif
else
install: error
all: error
error:
	$(error You must install portaudio19-dev to build $(MODNAME))
endif

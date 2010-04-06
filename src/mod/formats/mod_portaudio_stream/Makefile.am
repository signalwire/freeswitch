include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_portaudio_stream
PA_DIR=$(switch_srcdir)/libs/portaudio
PALA=$(switch_builddir)/libs/portaudio/lib/libportaudio.la
MODPA_DIR=$(switch_srcdir)/src/mod/endpoints/mod_portaudio

mod_LTLIBRARIES = mod_portaudio_stream.la
mod_portaudio_stream_la_SOURCES = mod_portaudio_stream.c $(MODPA_DIR)/pablio.c $(MODPA_DIR)/pa_ringbuffer.c
mod_portaudio_stream_la_CFLAGS = -I. -I$(PA_DIR)/include -D__EXTENSION__=1 -I$(MODPA_DIR) $(AM_CFLAGS)
mod_portaudio_stream_la_LIBADD = $(switch_builddir)/libfreeswitch.la $(PALA)
mod_portaudio_stream_la_LDFLAGS = -avoid-version -module -no-undefined -shared $(PA_LIBS)

if ISMAC
mod_portaudio_stream_la_LDFLAGS += -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework Carbon
endif

BUILT_SOURCES = $(PALA)

$(mod_portaudio_stream_la_SOURCES) : $(BUILT_SOURCES)

$(PALA): $(PA_DIR) $(PA_DIR)/.update
	cd $(switch_builddir)/libs/portaudio && $(MAKE) PORTAUDIO_CFLAGS="$(SWITCH_AM_CFLAGS)"
	$(TOUCH_TARGET)


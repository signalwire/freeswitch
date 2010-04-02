include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_portaudio
PA_DIR=$(switch_srcdir)/libs/portaudio
PALA=$(switch_builddir)/libs/portaudio/lib/libportaudio.la

mod_LTLIBRARIES = mod_portaudio.la
mod_portaudio_la_SOURCES = mod_portaudio.c pablio.c pa_ringbuffer.c
mod_portaudio_la_CFLAGS = -I. -I$(PA_DIR)/include -D__EXTENSION__=1 $(AM_CFLAGS)
mod_portaudio_la_LIBADD = $(switch_builddir)/libfreeswitch.la $(PALA)
mod_portaudio_la_LDFLAGS = -avoid-version -module -no-undefined -shared $(PA_LIBS)

if ISMAC
mod_portaudio_la_LDFLAGS += -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework Carbon
endif

BUILT_SOURCES = $(PALA)

$(mod_portaudio_la_SOURCES) : $(BUILT_SOURCES)

$(PALA): $(PA_DIR) $(PA_DIR)/.update
	cd $(switch_builddir)/libs/portaudio && $(MAKE) PORTAUDIO_CFLAGS="$(SWITCH_AM_CFLAGS)"
	$(TOUCH_TARGET)


include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_pulseaudio

if HAVE_PULSEAUDIO
mod_LTLIBRARIES = mod_pulseaudio.la
mod_pulseaudio_la_SOURCES  = mod_pulseaudio.c pablio.c
mod_pulseaudio_la_CFLAGS   = $(AM_CFLAGS) -I. -D__EXTENSION__=1 `pkg-config --cflags libpulse-simple`
mod_pulseaudio_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_pulseaudio_la_LDFLAGS  = -avoid-version -module -no-undefined -shared $(PULSEAUDIO_LIBS)
else
install: error
all: error
error:
	$(error You must install libpulse-dev to build $(MODNAME))
endif

include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_signalwire

if HAVE_KS
if HAVE_SIGNALWIRE_CLIENT

mod_LTLIBRARIES = mod_signalwire.la
mod_signalwire_la_SOURCES  = mod_signalwire.c
mod_signalwire_la_CFLAGS   = $(AM_CFLAGS)
mod_signalwire_la_CFLAGS  += $(CURL_CFLAGS) $(KS_CFLAGS) $(SIGNALWIRE_CLIENT_CFLAGS)
mod_signalwire_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_signalwire_la_LDFLAGS  = $(CURL_LIBS) -avoid-version -module -no-undefined -shared $(KS_LIBS) $(SIGNALWIRE_CLIENT_LIBS)

else
install: error
all: error
error:
	$(error You must install signalwire-client-c to build mod_signalwire)
endif

else
install: error
all: error
error:
	$(error You must install libks to build mod_signalwire)
endif


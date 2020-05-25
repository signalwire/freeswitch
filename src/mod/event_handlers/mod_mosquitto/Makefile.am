include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_mosquitto

if HAVE_MOSQUITTO

mod_LTLIBRARIES = mod_mosquitto.la
mod_mosquitto_la_SOURCES  = mosquitto_mosq.c mosquitto_events.c mosquitto_utils.c mosquitto_config.c mosquitto_cli.c mod_mosquitto.c
mod_mosquitto_la_CFLAGS   = $(AM_CFLAGS) $(OPENSSL_CFLAGS) $(MOSQUITTO_CFLAGS)
mod_mosquitto_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_mosquitto_la_LDFLAGS  = -avoid-version -module -no-undefined -shared $(OPENSSL_LIBS) $(MOSQUITTO_LIBS) $(SWITCH_AM_LDFLAGS)

else
install: error
all: error
error:
    $(error You must install libmosquitto1 and libmosquitto-dev to build this module)
endif
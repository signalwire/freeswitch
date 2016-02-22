include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_sms_flowroute

if HAVE_H2O

mod_LTLIBRARIES = mod_sms_flowroute.la
mod_sms_flowroute_la_SOURCES  = mod_sms_flowroute.c
mod_sms_flowroute_la_CFLAGS   = $(AM_CFLAGS) $(H2O_CFLAGS) $(BROTLIENC_CFLAGS) $(BROTLIDEC_CFLAGS)
mod_sms_flowroute_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_sms_flowroute_la_LDFLAGS  = -avoid-version -module -no-undefined -shared $(H2O_LIBS) $(BROTLIENC_LIBS) $(BROTLIDEC_LIBS) $(SWITCH_AM_LDFLAGS)

else
install: error
all: error
error:
	$(error You must install libh2o and libh2o-dev to build this module)
endif

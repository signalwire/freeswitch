include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_smpp

if HAVE_SMPP34

mod_LTLIBRARIES = mod_smpp.la
mod_smpp_la_SOURCES  = mod_smpp_utils.c mod_smpp_gateway.c mod_smpp_message.c mod_smpp.c
mod_smpp_la_CFLAGS   = $(AM_CFLAGS) $(SMPP34_CFLAGS)
mod_smpp_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_smpp_la_LDFLAGS  = -avoid-version -module -no-undefined -shared $(SMPP34_LIBS) $(SWITCH_AM_LDFLAGS)

else
install: error
all: error
error:
	$(error You must install libsmpp34-dev to build this module)
endif

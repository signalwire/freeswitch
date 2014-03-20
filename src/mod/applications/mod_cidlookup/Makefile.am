include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_cidlookup

mod_LTLIBRARIES = mod_cidlookup.la
mod_cidlookup_la_SOURCES  = mod_cidlookup.c
mod_cidlookup_la_CFLAGS   = $(AM_CFLAGS)
mod_cidlookup_la_CPPFLAGS   = $(CURL_CFLAGS) $(AM_CPPFLAGS)
mod_cidlookup_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_cidlookup_la_LDFLAGS  = $(CURL_LIBS) -avoid-version -module -no-undefined -shared

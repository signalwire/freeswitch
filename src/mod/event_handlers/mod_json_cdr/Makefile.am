include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_json_cdr

mod_LTLIBRARIES = mod_json_cdr.la
mod_json_cdr_la_SOURCES  = mod_json_cdr.c
mod_json_cdr_la_CFLAGS   = $(AM_CFLAGS)
mod_json_cdr_la_CPPFLAGS = $(CURL_CFLAGS) $(AM_CPPFLAGS)
mod_json_cdr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_json_cdr_la_LDFLAGS  = $(CURL_LIBS) -avoid-version -module -no-undefined -shared

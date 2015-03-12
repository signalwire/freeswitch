include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_curl

mod_LTLIBRARIES = mod_curl.la
mod_curl_la_SOURCES  = mod_curl.c
mod_curl_la_CFLAGS   = $(AM_CFLAGS)
mod_curl_la_CPPFLAGS = $(CURL_CFLAGS) $(AM_CPPFLAGS)
mod_curl_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_curl_la_LDFLAGS  = $(CURL_LIBS) -avoid-version -module -no-undefined -shared

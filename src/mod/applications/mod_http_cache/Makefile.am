include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_http_cache

mod_LTLIBRARIES = mod_http_cache.la
mod_http_cache_la_SOURCES  = mod_http_cache.c aws.c
mod_http_cache_la_CFLAGS   = $(AM_CFLAGS)
mod_http_cache_la_CPPFLAGS = $(CURL_CFLAGS) $(AM_CPPFLAGS)
mod_http_cache_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_http_cache_la_LDFLAGS  = $(CURL_LIBS) -avoid-version -module -no-undefined -shared

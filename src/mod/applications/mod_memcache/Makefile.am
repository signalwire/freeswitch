include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_memcache

if HAVE_MEMCACHED

mod_LTLIBRARIES = mod_memcache.la
mod_memcache_la_SOURCES  = mod_memcache.c
mod_memcache_la_CFLAGS   = $(AM_CFLAGS) $(MEMCACHED_CFLAGS)
mod_memcache_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(MEMCACHED_LIBS)
mod_memcache_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

else
install: error
all: error
error:
	$(error You must install libmemcached-dev to build mod_memcache)
endif

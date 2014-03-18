include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_memcache
MEMCACHED=libmemcached-0.32
MEMCACHED_DIR=$(switch_srcdir)/libs/$(MEMCACHED)
MEMCACHED_BUILDDIR=$(switch_builddir)/libs/$(MEMCACHED)

MEMCACHEDLA=$(MEMCACHED_BUILDDIR)/libmemcached/libmemcached.la

mod_LTLIBRARIES = mod_memcache.la
mod_memcache_la_SOURCES  = mod_memcache.c
mod_memcache_la_CFLAGS   = $(AM_CFLAGS) -I$(MEMCACHED_DIR) -I$(MEMCACHED_BUILDDIR)
mod_memcache_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(MEMCACHEDLA)
mod_memcache_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

MY_DEFAULT_ARGS=--build=$(build) --host=$(host) --target=$(target) --disable-shared --with-pic

BUILT_SOURCES=$(MEMCACHEDLA)

$(MEMCACHED_DIR):
	$(GETLIB) $(MEMCACHED).tar.gz

$(MEMCACHED_BUILDDIR)/Makefile: $(MEMCACHED_DIR)
	mkdir -p $(MEMCACHED_BUILDDIR)
	echo $(MY_DEFAULT_ARGS)
	cd $(MEMCACHED_BUILDDIR) && $(DEFAULT_VARS) $(MEMCACHED_DIR)/configure $(MY_DEFAULT_ARGS) --with-memcached=no --srcdir=$(MEMCACHED_DIR) CPPFLAGS= LDFLAGS=
	$(TOUCH_TARGET)

$(MEMCACHEDLA): $(MEMCACHED_BUILDDIR)/Makefile
	cd $(MEMCACHED_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

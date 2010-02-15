MEMCACHED=libmemcached-0.32
BASE=../../../..

WANT_CURL=yes

MEMCACHED_DIR=$(switch_srcdir)/libs/$(MEMCACHED)
MEMCACHED_BUILDDIR=$(switch_builddir)/libs/$(MEMCACHED)

MEMCACHEDLA=$(MEMCACHED_BUILDDIR)/libmemcached/libmemcached.la

LOCAL_CFLAGS=-I$(MEMCACHED_DIR) -I$(MEMCACHED_BUILDDIR)
LOCAL_LIBADD=$(MEMCACHEDLA)

include $(BASE)/build/modmake.rules

MY_DEFAULT_ARGS:= --build=$(build) --host=$(host) --target=$(target) \
  --disable-shared --with-pic

$(MEMCACHED_DIR):
	$(GETLIB) $(MEMCACHED).tar.gz

$(MEMCACHED_BUILDDIR)/Makefile: $(MEMCACHED_DIR)
	mkdir -p $(MEMCACHED_BUILDDIR)
	echo $(MY_DEFAULT_ARGS)
	cd $(MEMCACHED_BUILDDIR) && $(DEFAULT_VARS) $(MEMCACHED_DIR)/configure $(MY_DEFAULT_ARGS) --with-memcached=no --srcdir=$(MEMCACHED_DIR)  CPPFLAGS= LDFLAGS=
	$(TOUCH_TARGET)

$(MEMCACHEDLA): $(MEMCACHED_BUILDDIR)/Makefile
	cd $(MEMCACHED_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)




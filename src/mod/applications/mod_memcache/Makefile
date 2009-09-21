MEMCACHED=libmemcached-0.32
switch_srcdir=../../../..

WANT_CURL=yes

MEMCACHED_DIR=$(switch_srcdir)/libs/$(MEMCACHED)

MEMCACHEDLA=$(MEMCACHED_DIR)/libmemcached/libmemcached.la

LOCAL_CFLAGS=-I$(MEMCACHED_DIR)
LOCAL_LIBADD=$(MEMCACHEDLA)

include $(switch_srcdir)/build/modmake.rules

DEFAULT_ARGS=--prefix=$(PREFIX) --disable-shared --with-pic

$(LOCAL_OBJS): $(LOCAL_SOURCES)

$(MEMCACHED_DIR):
	$(GETLIB) $(MEMCACHED).tar.gz

$(MEMCACHED_DIR)/Makefile: $(MEMCACHED_DIR)
	cd $(MEMCACHED_DIR) && CFLAGS=$(AM_CFLAGS) CC=$(CC) CXX=$(CXX) ./configure --disable-shared --with-pic --with-memcached=no CPPFLAGS= LDFLAGS=
	$(TOUCH_TARGET)

$(MEMCACHEDLA): $(MEMCACHED_DIR)/Makefile
	cd $(MEMCACHED_DIR) && $(MAKE)
	$(TOUCH_TARGET)




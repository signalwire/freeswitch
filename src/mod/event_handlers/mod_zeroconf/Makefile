
switch_srcdir=../../../..

HOWL=howl-1.0.0
HOWL_DIR=$(switch_srcdir)/libs/$(HOWL)
HOWLLA=$(HOWL_DIR)/src/lib/howl/libhowl.la

LOCAL_CFLAGS=-I$(HOWL_DIR)/include
LOCAL_LIBADD=$(HOWLLA)

DEFAULT_ARGS=--prefix=$(PREFIX) --disable-shared --with-pic

include $(switch_srcdir)/build/modmake.rules

$(HOWL_DIR)/Makefile:
	$(GETLIB) $(HOWL).tar.gz
	cd $(HOWL_DIR) && ./configure $(DEFAULT_ARGS)

$(HOWLLA): $(HOWL_DIR)/Makefile
	cd $(HOWL_DIR) && $(MAKE)

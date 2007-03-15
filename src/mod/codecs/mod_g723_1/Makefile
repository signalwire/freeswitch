MODNAME=mod_g723_1
BASE=../../../..
topdir=$(shell cd $(BASE) && pwd)
ifeq ($(shell test -d $(topdir)/libs/libg723_1 && cd $(topdir)/libs/libg723_1 && pwd),$(topdir)/libs/libg723_1)
LOCAL_CFLAGS += -I$(PREFIX)/include/g723
LOCAL_LDFLAGS +=-lg723
else
LOCAL_CFLAGS += -DG723_PASSTHROUGH
endif
include $(BASE)/build/modmake.rules

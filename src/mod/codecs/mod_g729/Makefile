
BASE=../../../..
topdir=$(shell cd $(BASE) && pwd)
ifeq ($(shell test -d $(topdir)/libs/libg729 && cd $(topdir)/libs/libg729 && pwd),$(topdir)/libs/libg729)
LOCAL_CFLAGS += -I$(PREFIX)/include/libg729
LOCAL_LDFLAGS +=-lg729
else
LOCAL_CFLAGS += -DG729_PASSTHROUGH
endif

include $(BASE)/build/modmake.rules

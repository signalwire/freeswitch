MODNAME=mod_g723_1
ifeq ($(shell ls -d $(BASE)/libs/libg723_1),$(BASE)/libs/libg723_1)
LOCAL_CFLAGS += -I$(PREFIX)/include/g723
LOCAL_LDFLAGS +=-lg723
else
LOCAL_CFLAGS += -DG723_PASSTHROUGH
endif
include ../../../../build/modmake.rules

ifeq ($(shell ls -d $(BASE)/libs/libg729),$(BASE)/libs/libg729)
LOCAL_CFLAGS += -I$(PREFIX)/include/libg729
LOCAL_LDFLAGS +=-lg729
else
LOCAL_CFLAGS += -DG729_PASSTHROUGH
endif

include $(BASE)/build/modmake.rules

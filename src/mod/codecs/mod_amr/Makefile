ifeq ($(shell ls -d $(BASE)/libs/amr),$(BASE)/libs/amr)

LOCAL_CFLAGS += -I$(PREFIX)/include/amr
LOCAL_LDFLAGS +=-lamr

else

LOCAL_CFLAGS += -DAMR_PASSTHROUGH

endif

include $(BASE)/build/modmake.rules

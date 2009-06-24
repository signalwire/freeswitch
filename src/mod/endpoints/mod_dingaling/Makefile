BASE=../../../..

LOCAL_CFLAGS     = `$(BASE)/libs/apr/apr-1-config --cflags --cppflags --includes`
LOCAL_CFLAGS    += `$(BASE)/libs/apr-util/apu-1-config --includes`
#LOCAL_CFLAGS += -w

IKS_DIR=$(BASE)/libs/iksemel
IKS_LA=$(IKS_DIR)/src/libiksemel.la
DING_DIR=$(BASE)/libs/libdingaling
LOCAL_CFLAGS += -I$(DING_DIR)/src -I$(BASE)/libs/iksemel/include
LOCAL_OBJS=$(DING_DIR)/src/libdingaling.o $(DING_DIR)/src/sha1.o $(IKS_LA)
LOCAL_SOURCES=$(DING_DIR)/src/libdingaling.c $(DING_DIR)/src/sha1.c
include $(BASE)/build/modmake.rules

$(IKS_LA): $(IKS_DIR) $(IKS_DIR)/.update
	@cd $(IKS_DIR) && $(MAKE)
	@$(TOUCH_TARGET)





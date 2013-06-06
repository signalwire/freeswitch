BASE=../../../..

IKS_DIR=$(BASE)/libs/iksemel
IKS_LA=$(IKS_DIR)/src/libiksemel.la
LOCAL_CFLAGS += -I$(BASE)/libs/iksemel/include
LOCAL_OBJS= $(IKS_LA)
include $(BASE)/build/modmake.rules

$(IKS_LA): $(IKS_DIR) $(IKS_DIR)/.update
	@cd $(IKS_DIR) && $(MAKE)
	@$(TOUCH_TARGET)

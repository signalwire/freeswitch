BASE=../../../../..

IKS_DIR=$(BASE)/libs/iksemel
PCRE_DIR=$(BASE)/libs/pcre
IKS_LA=$(IKS_DIR)/src/libiksemel.la
PCRE_LA=$(PCRE_DIR)/libpcre.la
LOCAL_CFLAGS += -I../ -I$(BASE)/libs/iksemel/include -I$(BASE)/libs/pcre/include
LOCAL_OBJS= $(PCRE_LA) $(IKS_LA) main.o ../srgs.o
LOCAL_SOURCES= main.c
include $(BASE)/build/modmake.rules

$(IKS_LA): $(IKS_DIR) $(IKS_DIR)/.update
	@cd $(IKS_DIR) && $(MAKE)
	@$(TOUCH_TARGET)

$(PCRE_LA): $(PCRE_DIR) $(PCRE_DIR)/.update
	@cd $(PCRE_DIR) && $(MAKE)
	@$(TOUCH_TARGET)

local_all:
	libtool --mode=link gcc main.o ../srgs.o -o test test_srgs.la

local_clean:
	-rm test

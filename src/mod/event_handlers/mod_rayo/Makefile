BASE=../../../..

IKS_DIR=$(BASE)/libs/iksemel
PCRE_DIR=$(BASE)/libs/pcre
IKS_LA=$(IKS_DIR)/src/libiksemel.la
PCRE_LA=$(PCRE_DIR)/libpcre.la
LOCAL_CFLAGS += -I$(BASE)/libs/iksemel/include -I$(BASE)/libs/pcre
LOCAL_OBJS= $(IKS_LA) \
	$(PCRE_LA) \
	iks_helpers.o \
	nlsml.o \
	rayo_components.o \
	rayo_elements.o \
	rayo_input_component.o \
	rayo_output_component.o \
	rayo_prompt_component.o \
	rayo_receivefax_component.o \
	rayo_record_component.o \
	sasl.o \
	srgs.o \
	xmpp_streams.o
LOCAL_SOURCES=	\
	iks_helpers.c \
	nlsml.c \
	rayo_components.c \
	rayo_elements.c \
	rayo_input_component.c \
	rayo_output_component.c \
	rayo_prompt_component.c \
	rayo_record_component.c \
	rayo_receivefax_component.c \
	sasl.c \
	srgs.c \
	xmpp_streams.c
include $(BASE)/build/modmake.rules

$(IKS_LA): $(IKS_DIR) $(IKS_DIR)/.update
	@cd $(IKS_DIR) && $(MAKE)
	@$(TOUCH_TARGET)

$(PCRE_LA): $(PCRE_DIR) $(PCRE_DIR)/.update
	@cd $(PCRE_DIR) && $(MAKE)
	@$(TOUCH_TARGET)

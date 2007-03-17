BASE=../../../..

OS_ARCH         := $(subst /,_,$(shell uname -s | sed /\ /s//_/))

SOFIA_DIR=$(BASE)/libs/sofia-sip
SOFIAUA_DIR=$(SOFIA_DIR)/libsofia-sip-ua

LOCAL_CFLAGS  = -I. -I$(SOFIAUA_DIR)/bnf  -I$(SOFIAUA_DIR)/features
LOCAL_CFLAGS += -I$(SOFIAUA_DIR)/http -I$(SOFIAUA_DIR)/ipt
LOCAL_CFLAGS += -I$(SOFIAUA_DIR)/iptsec -I$(SOFIAUA_DIR)/msg
LOCAL_CFLAGS += -I$(SOFIAUA_DIR)/nea -I$(SOFIAUA_DIR)/nta
LOCAL_CFLAGS += -I$(SOFIAUA_DIR)/nth -I$(SOFIAUA_DIR)/nua
LOCAL_CFLAGS += -I$(SOFIAUA_DIR)/sdp -I$(SOFIAUA_DIR)/sip
LOCAL_CFLAGS += -I$(SOFIAUA_DIR)/soa -I$(SOFIAUA_DIR)/sresolv
LOCAL_CFLAGS += -I$(SOFIAUA_DIR)/stun -I$(SOFIAUA_DIR)/su
LOCAL_CFLAGS += -I$(SOFIAUA_DIR)/tport -I$(SOFIAUA_DIR)/url

SOFIALA=$(SOFIAUA_DIR)/libsofia-sip-ua.la

LOCAL_LIBADD=$(SOFIALA)

ifeq ($(OS_ARCH),Darwin)
	LOCAL_LDFLAGS +=-framework CoreFoundation -framework SystemConfiguration
endif

include $(BASE)/build/modmake.rules

$(SOFIALA): $(SOFIA_DIR) $(SOFIA_DIR)/.update
	cd $(SOFIA_DIR) && $(MAKE)
	$(TOUCH_TARGET)

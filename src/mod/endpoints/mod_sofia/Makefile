MODNAME=mod_sofia
OS_ARCH         := $(subst /,_,$(shell uname -s | sed /\ /s//_/))

LOCAL_CFLAGS  = -I. -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/bnf  -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/features
LOCAL_CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/http -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/ipt
LOCAL_CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/iptsec -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/msg
LOCAL_CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/nea -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/nta
LOCAL_CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/nth -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/nua
LOCAL_CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/sdp -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/sip
LOCAL_CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/soa -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/sresolv
LOCAL_CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/stun -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/su
LOCAL_CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/tport -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/url

LOCAL_LDFLAGS=$(BASE)/libs/sofia-sip/libsofia-sip-ua/libsofia-sip-ua.la

ifeq ($(OS_ARCH),Darwin)
	LOCAL_LDFLAGS +=-framework CoreFoundation -framework SystemConfiguration
endif

include ../../../../build/modmake.rules

local_depend:
	cd $(BASE)/libs/sofia-sip && $(MAKE)

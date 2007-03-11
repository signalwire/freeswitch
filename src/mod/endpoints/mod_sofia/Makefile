OS_ARCH         := $(subst /,_,$(shell uname -s | sed /\ /s//_/))
LINKER=$(CC)
CFLAGS += -I. -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/bnf  -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/features
CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/http -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/ipt
CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/iptsec -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/msg
CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/nea -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/nta
CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/nth -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/nua
CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/sdp -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/sip
CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/soa -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/sresolv
CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/stun -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/su
CFLAGS += -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/tport -I$(BASE)/libs/sofia-sip/libsofia-sip-ua/url

ifeq ($(OS_ARCH),Darwin)
	LDFLAGS +=-framework CoreFoundation -framework SystemConfiguration
endif
all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	cd $(BASE)/libs/sofia-sip && $(MAKE)

%.o:  %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -c $(MODNAME).c -o $(MODNAME).o 
	$(LINK) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(MODNAME).o $(LDFLAGS) $(BASE)/libs/sofia-sip/libsofia-sip-ua/libsofia-sip-ua.la

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ .libs

install:
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod

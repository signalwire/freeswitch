CFLAGS += -I$(BASE)/libs/udns
LINKER=$(CC)

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	cd $(BASE)/libs/udns && $(MAKE) staticlib

%.o:  %.c
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -c $(MODNAME).c -o $(MODNAME).o 
	$(LINK) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(MODNAME).o $(LDFLAGS) $(BASE)/libs/udns/libudns.a

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod

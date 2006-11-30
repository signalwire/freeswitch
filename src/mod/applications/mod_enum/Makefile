CFLAGS += -I$(BASE)/libs/udns
LDFLAGS += -L$(BASE)/libs/udns -ludns
LINKER=$(CC)

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) udns

%.o:  %.c
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod

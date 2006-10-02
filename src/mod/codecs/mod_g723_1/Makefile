ifeq ($(shell ls $(BASE)/libs/libg723_1),$(BASE)/libs/libg723_1)

CFLAGS += -I$(PREFIX)/include/g723
LDFLAGS +=-lg723

else

CFLAGS += -DG723_PASSTHROUGH
endif

all:	$(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o
	$(CC) $(SOLINK) $(MODNAME).o -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod

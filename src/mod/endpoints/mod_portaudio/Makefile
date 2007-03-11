CFLAGS += -I. -I$(BASE)/libs/portaudio/include
MYOBJS = pablio.o pa_ringbuffer.o
ifeq ($(OSARCH),Darwin)
LDFLAGS += -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework Carbon
endif

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	cd $(BASE)/libs/portaudio && $(MAKE)

%.o:  %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c $(MYOBJS)
	$(CC) $(CFLAGS) -c $(MODNAME).c -o $(MODNAME).o 
	$(LINK) $(SOLINK) $(MODNAME).o -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(MYOBJS) $(LDFLAGS) $(BASE)/libs/portaudio/lib/libportaudio.la



clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ .libs

install:
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod

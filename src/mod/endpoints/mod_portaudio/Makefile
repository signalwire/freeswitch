LDFLAGS += -lportaudio -L/usr/local/lib
MYOBJS = pablio.o ringbuffer.o

all:	depends $(MODNAME).so

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install portaudio.tar.gz --prefix=$(PREFIX)
%.o:  %.c
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

$(MODNAME).so: $(MODNAME).c $(MYOBJS)
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(CC) $(SOLINK) -o $(MODNAME).so $(MODNAME).o $(MYOBJS) $(LDFLAGS)



clean:
	rm -fr *.so *.o *~

install:
	cp -f $(MODNAME).so $(PREFIX)/mod

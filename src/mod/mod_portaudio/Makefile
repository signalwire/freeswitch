LDFLAGS += -lportaudio -L/usr/local/lib
MYOBJS = pablio.o ringbuffer.o

all:	depends $(MOD).so

depends:
	$(BASE)/buildlib.sh $(BASE) install portaudio.tar.gz
%.o:  %.c
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

$(MOD).so: $(MOD).c $(MYOBJS)
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o 
	$(CC) $(SOLINK) -o $(MOD).so $(MOD).o $(MYOBJS) $(LDFLAGS)



clean:
	rm -fr *.so *.o *~


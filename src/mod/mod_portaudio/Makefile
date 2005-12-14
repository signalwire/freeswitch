LDFLAGS += -lportaudio -L/usr/local/lib
MYOBJS = pablio.o ringbuffer.o
%.o:  %.c
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

$(MOD).so: $(MOD).c $(MYOBJS)
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o 
	$(CC) $(SOLINK) -o $(MOD).so $(MOD).o $(MYOBJS) $(LDFLAGS)

all:	$(MOD).so

clean:
	rm -fr *.so *.o *~


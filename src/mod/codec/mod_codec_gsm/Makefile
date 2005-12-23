CFLAGS += -I/usr/local/include/libgsm
LDFLAGS +=-lgsm

$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o
	$(CC) $(SOLINK) $(MOD).o -o $(MOD).so $(LDFLAGS) -lspeex

all:	$(MOD).so

clean:
	rm -fr *.so *.o *~


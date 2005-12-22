CFLAGS += -I/usr/local/include/libg729
LDFLAGS +=-lg729

all:	$(MOD).so

$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o
	$(CC) $(SOLINK) $(MOD).o -o $(MOD).so $(LDFLAGS) -lspeex

clean:
	rm -fr *.so *.o *~


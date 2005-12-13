CFLAGS += -I/usr/local/include/libg729
LDFLAGS +=-lg729

$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o
	$(CC) -shared -Xlinker -x $(MOD).o -o $(MOD).so $(LDFLAGS) -lspeex

all:	$(MOD).so

clean:
	rm -fr *.so *.o *~


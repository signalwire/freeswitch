LDFLAGS += -liax -L/usr/local/lib

$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o 
	$(CC) -shared -Xlinker -x -o $(MOD).so  $(MOD).o $(LDFLAGS)

all:	$(MOD).so

clean:
	rm -fr *.so *.o *~


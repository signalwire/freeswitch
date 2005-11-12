
all:	$(MOD).so

g711.o: g711.c g711.h
	$(CC) -c -O2 -pthread -DLINUX=2 -D_REENTRANT -D_GNU_SOURCE -D_LARGEFILE64_SOURCE g711.c -o g711.o

$(MOD).so: $(MOD).c g711.o
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o
	$(CC) -shared -Xlinker -x g711.o $(MOD).o -o $(MOD).so $(LDFLAGS)



clean:
	rm -fr *.so *.o *~


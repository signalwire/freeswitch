
all:	$(MODNAME).so

g711.o: g711.c g711.h
	$(CC) $(CFLAGS) -c -O2 -pthread -DLINUX=2 -D_REENTRANT -D_GNU_SOURCE -D_LARGEFILE64_SOURCE g711.c -o g711.o

$(MODNAME).so: $(MODNAME).c g711.o
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o
	$(CC) $(SOLINK) g711.o $(MODNAME).o -o $(MODNAME).so $(LDFLAGS)



clean:
	rm -fr *.so *.o *~

install:
	cp -f $(MODNAME).so $(PREFIX)/mod


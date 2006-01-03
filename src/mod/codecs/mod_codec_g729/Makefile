CFLAGS += -I/usr/local/include/libg729
LDFLAGS +=-lg729

all:	$(MODNAME).so

$(MODNAME).so: $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o
	$(CC) $(SOLINK) $(MODNAME).o -o $(MODNAME).so $(LDFLAGS) -lspeex

clean:
	rm -fr *.so *.o *~

install:
	cp -f $(MODNAME).so $(PREFIX)/mod

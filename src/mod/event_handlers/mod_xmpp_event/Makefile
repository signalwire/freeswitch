LDFLAGS += -liksemel -L/usr/local/lib

all:	depends $(MOD).so

depends:
	$(BASE)/buildlib.sh $(BASE) install iksemel-1.2.tar.gz --prefix=$(PREFIX)

$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o 
	$(CC) $(SOLINK) -o $(MOD).so  $(MOD).o $(LDFLAGS)

clean:
	rm -fr *.so *.o *~


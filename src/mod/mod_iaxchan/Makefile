LDFLAGS += -liax -L/usr/local/lib

depends:
	$(BASE)/buildlib.sh $(BASE) install iax --enable-newjb

$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o 
	$(CC) $(SOLINK) -o $(MOD).so  $(MOD).o $(LDFLAGS)

all:	depends $(MOD).so

clean:
	rm -fr *.so *.o *~


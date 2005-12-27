LDFLAGS += -lsndfile -L/usr/local/lib

all:	depends $(MOD).so

depends:
	$(BASE)/buildlib.sh $(BASE) install libsndfile-1.0.12.tar.gz

$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o 
	$(CC) $(SOLINK) -o $(MOD).so  $(MOD).o $(LDFLAGS)


clean:
	rm -fr *.so *.o *~


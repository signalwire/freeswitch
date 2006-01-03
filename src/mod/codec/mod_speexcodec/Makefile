all:	depends $(MOD).so

depends:
	$(BASE)/buildlib.sh $(BASE) install speex-1.1.11.1.tar.gz --prefix=$(PREFIX)

$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o
	$(CC) $(SOLINK) $(MOD).o -o $(MOD).so $(LDFLAGS) -lspeex

clean:
	rm -fr *.so *.o *~


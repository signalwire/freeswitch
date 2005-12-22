all:	$(MOD).so

$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o
	$(CC) $(SOLINK) $(MOD).o -o $(MOD).so $(LDFLAGS)

clean:
	rm -fr *.so *.o *~


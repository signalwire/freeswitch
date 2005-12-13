
$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o
	$(CC) $(SOLINK) $(MOD).o -o $(MOD).so $(LDFLAGS)

all:	$(MOD).so

clean:
	rm -fr *.so *.o *~


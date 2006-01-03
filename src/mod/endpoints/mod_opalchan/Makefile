#CFLAGS += -I/usr/src/common/src
LDFLAGS += -liax

all:	$(MOD).so

$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o 
	$(CC) $(SOLINK) -o $(MOD).so  $(MOD).o $(LDFLAGS)

clean:
	rm -fr *.so *.o *~


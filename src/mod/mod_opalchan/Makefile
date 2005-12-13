#CFLAGS += -I/usr/src/common/src
LDFLAGS += -liax

$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o 
	$(CC) $(SOLINK) -o $(MOD).so  $(MOD).o $(LDFLAGS)

all:	$(MOD).so

clean:
	rm -fr *.so *.o *~


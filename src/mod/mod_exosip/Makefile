#CFLAGS += -I/usr/src/common/src
LDFLAGS += -leXosip2  -ljrtp4c

OSARCH=$(shell uname -s)
ifeq ($(OSARCH),Darwin)
	LINKER=g++
else
	LINKER=$(CC)
endif


$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o 
	$(LINKER) $(SOLINK) -o $(MOD).so  $(MOD).o $(LDFLAGS)

all:	$(MOD).so

clean:
	rm -fr *.so *.o *~


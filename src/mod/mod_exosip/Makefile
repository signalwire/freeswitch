#CFLAGS += -I/usr/src/common/src
LDFLAGS += -leXosip2  -ljrtp4c

ifeq ($(OSARCH),Darwin)
	LINKER=g++
else
	LINKER=$(CC)
endif

depends:
	$(BASE)/buildlib.sh $(BASE) install jthread-1.1.2.tar.gz --prefix=/usr/local 
	$(BASE)/buildlib.sh $(BASE) install jrtplib-3.3.0.tar.gz --prefix=/usr/local 
	$(BASE)/buildlib.sh $(BASE) install jrtp4c
	$(BASE)/buildlib.sh $(BASE) install libosip2-2.2.2.tar.gz --prefix=/usr/local 
	$(BASE)/buildlib.sh $(BASE) install libeXosip2-2.2.2.tar.gz --prefix=/usr/local --disable-josua


$(MOD).so: $(MOD).c
	$(CC) $(CFLAGS) -fPIC -c $(MOD).c -o $(MOD).o 
	$(LINKER) $(SOLINK) -o $(MOD).so  $(MOD).o $(LDFLAGS)

all:	depends $(MOD).so

clean:
	rm -fr *.so *.o *~


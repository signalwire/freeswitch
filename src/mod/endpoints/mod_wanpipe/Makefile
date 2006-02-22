CFLAGS +=-I/usr/local/include -I/usr/src/libpri -I/usr/src/linux/include -I. -I/usr/include
CFLAGS +=-D__LINUX__ -D_REENTRANT -D_GNU_SOURCE -O6 -march=i686 -DAFT_A104
LDFLAGS += -lsangoma


ifeq ($(OSARCH),Darwin)
	LINKER=g++
else
	LINKER=$(CC)
endif

all:	depends $(MODNAME).so

depends:

$(MODNAME).so: $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).so  $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.so *.o *~

install:
	cp -f $(MODNAME).so $(PREFIX)/mod

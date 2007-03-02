CFLAGS +=-I/usr/local/include -I/usr/src/libpri -I/usr/src/linux/include -I. -I/usr/include
CFLAGS +=-D__LINUX__ -D_REENTRANT -D_GNU_SOURCE -DAFT_A104 -DWANPIPE_TDM_API
LDFLAGS += -lsangoma
OBJS = ss7boost_client.o mod_wanpipe.o

ifeq ($(OSARCH),Darwin)
	LINKER=g++
else
	LINKER=$(CC)
endif

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install libsangoma --prefix=$(PREFIX) --with-libpri=/usr/src/libpri

%.o:  %.c
	$(CC) $(CFLAGS) $(MOD_CFLAGS) -g -ggdb -Werror -fPIC -c $< -o $@ 

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(OBJS) $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(OBJS) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod

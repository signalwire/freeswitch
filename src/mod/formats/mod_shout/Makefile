LDFLAGS += -lshout -lmp3lame
MOD_CFLAGS += "-I${PREFIX}/include"
MOD_LDFLAGS += "-L${PREFIX}/lib"
all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install lame-3.97.tar.gz --prefix=$(PREFIX) --disable-shared --with-pic
	MOD_CFLAGS="${MOD_CFLAGS}" MOD_LDFLAGS="${LD_CFLAGS}" MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install libshout-2.2.2.tar.gz --disable-shared --with-pic --prefix=$(PREFIX)

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(CC) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(LDFLAGS)


clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod

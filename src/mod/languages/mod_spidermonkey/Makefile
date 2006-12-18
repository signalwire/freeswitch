include sm.mak

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install js/nsprpub --prefix=$(PREFIX) --with-pic
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install js --prefix=$(PREFIX) --with-pic --with-nspr=yes
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install curl-7.15.2.tar.gz --prefix=$(PREFIX) --without-libidn

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -c $(MODNAME).c -o $(MODNAME).o 
	$(CC) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(OBJS) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod

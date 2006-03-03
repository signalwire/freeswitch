ifeq ($(OSARCH),Darwin)
CFLAGS += -L/Library/Frameworks/swift.framework/Versions/4 -I/Library/Frameworks/swift.framework/Versions/4/Headers
LDFLAGS += -framework swift
else
LDFLAGS += -L/opt/swift/lib -lswift -lceplex_us -lceplang_en -lceplang_de -lceplang_es -lceplang_fr -lceplang_it -lm
CFLAGS += -I/opt/swift/include
endif

LINKER=$(CC)

all:	depends $(MODNAME).so

depends:

$(MODNAME).so: $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).so  $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.so *.o *~

install:
	cp -f $(MODNAME).so $(PREFIX)/mod

ifeq ($(OSARCH),Darwin)
CFLAGS += -I/Library/Frameworks/swift.framework/Versions/4/Headers
LDFLAGS += -framework swift
else
LDFLAGS += -L/opt/swift/lib -lswift -lceplex_us -lceplang_en -lceplang_de -lceplang_es -lceplang_fr -lceplang_it -lm
CFLAGS += -I/opt/swift/include
endif

LINKER=$(CC)

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod

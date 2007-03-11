ifeq ($(OSARCH),Darwin)
CFLAGS += -I/Library/Frameworks/swift.framework/Versions/4/Headers
LDFLAGS += -framework swift
else
LDFLAGS += -L/opt/swift/lib -lswift -lceplex_us -lceplang_en -lm
CFLAGS += -I/opt/swift/include
endif

LINKER=$(CC)

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -c $(MODNAME).c -o $(MODNAME).o 
	$(LINK) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ .libs

install:
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod

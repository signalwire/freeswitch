#LDFLAGS += -lxmlrpc -lxmlrpc_abyss -lxmlrpc_server -lxmlrpc_server_abyss -lxmlrpc_xmlparse
LDFLAGS += $(BASE)/libs/xmlrpc-c/src/libxmlrpc.la $(BASE)/libs/xmlrpc-c/src/libxmlrpc_server.la 
LDFLAGS += $(BASE)/libs/xmlrpc-c/src/libxmlrpc_server_abyss.la $(BASE)/libs/xmlrpc-c/lib/abyss/src/libxmlrpc_abyss.la
LDFLAGS += $(BASE)/libs/xmlrpc-c/lib/expat/xmlparse/libxmlrpc_xmlparse.la

CFLAGS += -I$(BASE)/libs/xmlrpc-c/include

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	cd $(BASE)/libs/xmlrpc-c && $(MAKE)


$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -c $(MODNAME).c -o $(MODNAME).o 
	$(LINK) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(LDFLAGS)


clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ .libs

install:
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod

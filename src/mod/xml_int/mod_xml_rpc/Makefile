MODNAME=mod_xml_rpc
LOCAL_LDFLAGS  = $(BASE)/libs/xmlrpc-c/src/libxmlrpc.la $(BASE)/libs/xmlrpc-c/src/libxmlrpc_server.la 
LOCAL_LDFLAGS += $(BASE)/libs/xmlrpc-c/src/libxmlrpc_server_abyss.la $(BASE)/libs/xmlrpc-c/lib/abyss/src/libxmlrpc_abyss.la
LOCAL_LDFLAGS += $(BASE)/libs/xmlrpc-c/lib/expat/xmlparse/libxmlrpc_xmlparse.la

LOCAL_CFLAGS=-I$(BASE)/libs/xmlrpc-c/include

include ../../../../build/modmake.rules

local_depend:
	cd $(BASE)/libs/xmlrpc-c && $(MAKE)


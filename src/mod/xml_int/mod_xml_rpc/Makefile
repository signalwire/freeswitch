MODNAME=mod_xml_rpc
BASE=../../../..

XMLRPCLAS  = $(BASE)/libs/xmlrpc-c/src/libxmlrpc.la $(BASE)/libs/xmlrpc-c/src/libxmlrpc_server.la 
XMLRPCLAS += $(BASE)/libs/xmlrpc-c/src/libxmlrpc_server_abyss.la $(BASE)/libs/xmlrpc-c/lib/abyss/src/libxmlrpc_abyss.la
XMLRPCLAS += $(BASE)/libs/xmlrpc-c/lib/expat/xmlparse/libxmlrpc_xmlparse.la

LOCAL_LDFLAGS = $(XMLRPCLAS)
LOCAL_CFLAGS=-I$(BASE)/libs/xmlrpc-c/include

include ../../../../build/modmake.rules

local_depend: $(XMLRPCLAS)

$(XMLRPCLAS):
	cd $(BASE)/libs/xmlrpc-c && $(MAKE)


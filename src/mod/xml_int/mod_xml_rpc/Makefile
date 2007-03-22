BASE=../../../..
XMLRPC_DIR=$(BASE)/libs/xmlrpc-c

#XMLRPCLAS  = $(XMLRPC_DIR)/src/libxmlrpc.la 
#dont ask...
XMLRPCLAS  = $(XMLRPC_DIR)/src/libxmlrpc.la
XMLRPCLAS += $(XMLRPC_DIR)/src/libxmlrpc_server.la 
XMLRPCLAS += $(XMLRPC_DIR)/src/libxmlrpc_server_abyss.la $(XMLRPC_DIR)/lib/abyss/src/libxmlrpc_abyss.la
XMLRPCLAS += $(XMLRPC_DIR)/lib/expat/xmlparse/libxmlrpc_xmlparse.la $(XMLRPC_DIR)/lib/expat/xmltok/libxmlrpc_xmltok.la

LOCAL_LIBADD =$(XMLRPCLAS)
LOCAL_CFLAGS=-I$(XMLRPC_DIR)/include

include $(BASE)/build/modmake.rules

$(XMLRPCLAS): $(XMLRPC_DIR) $(XMLRPC_DIR)/.update
	cd $(XMLRPC_DIR) && $(MAKE)
	$(TOUCH_TARGET)

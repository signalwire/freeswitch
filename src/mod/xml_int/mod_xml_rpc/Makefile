BASE=../../../..
XMLRPC_DIR=$(BASE)/libs/xmlrpc-c

LOCAL_OBJS = $(XMLRPC_DIR)/src/version.o\
$(XMLRPC_DIR)/lib/libutil/asprintf.o\
$(XMLRPC_DIR)/lib/util/casprintf.o\
$(XMLRPC_DIR)/lib/abyss/src/channel.o\
$(XMLRPC_DIR)/lib/abyss/src/chanswitch.o\
$(XMLRPC_DIR)/lib/util/cmdline_parser.o\
$(XMLRPC_DIR)/lib/abyss/src/conf.o\
$(XMLRPC_DIR)/lib/abyss/src/conn.o\
$(XMLRPC_DIR)/lib/abyss/src/data.o\
$(XMLRPC_DIR)/lib/abyss/src/date.o\
$(XMLRPC_DIR)/src/double.o\
$(XMLRPC_DIR)/lib/libutil/error.o\
$(XMLRPC_DIR)/lib/abyss/src/file.o\
$(XMLRPC_DIR)/lib/expat/gennmtab/gennmtab.o\
$(XMLRPC_DIR)/lib/util/getoptx.o\
$(XMLRPC_DIR)/lib/abyss/src/handler.o\
$(XMLRPC_DIR)/lib/abyss/src/http.o\
$(XMLRPC_DIR)/lib/abyss/src/init.o\
$(XMLRPC_DIR)/lib/libutil/make_printable.o\
$(XMLRPC_DIR)/lib/libutil/memblock.o\
$(XMLRPC_DIR)/src/method.o\
$(XMLRPC_DIR)/src/parse_value.o\
$(XMLRPC_DIR)/src/parse_datetime.o\
$(XMLRPC_DIR)/src/registry.o\
$(XMLRPC_DIR)/src/resource.o\
$(XMLRPC_DIR)/lib/abyss/src/response.o\
$(XMLRPC_DIR)/lib/libutil/select.o\
$(XMLRPC_DIR)/lib/abyss/src/server.o\
$(XMLRPC_DIR)/lib/abyss/src/session.o\
$(XMLRPC_DIR)/lib/libutil/sleep.o\
$(XMLRPC_DIR)/lib/abyss/src/socket.o\
$(XMLRPC_DIR)/lib/abyss/src/socket_unix.o\
$(XMLRPC_DIR)/lib/util/string_parser.o\
$(XMLRPC_DIR)/lib/util/stripcaseeq.o\
$(XMLRPC_DIR)/src/system_method.o\
$(XMLRPC_DIR)/lib/abyss/src/thread_pthread.o\
$(XMLRPC_DIR)/lib/libutil/time.o\
$(XMLRPC_DIR)/lib/abyss/src/token.o\
$(XMLRPC_DIR)/src/trace.o\
$(XMLRPC_DIR)/lib/abyss/src/trace.o\
$(XMLRPC_DIR)/lib/libutil/utf8.o\
$(XMLRPC_DIR)/lib/libutil/string_number.o\
$(XMLRPC_DIR)/lib/libutil/base64.o\
$(XMLRPC_DIR)/lib/expat/xmlparse/xmlparse.o\
$(XMLRPC_DIR)/lib/expat/xmltok/xmlrole.o\
$(XMLRPC_DIR)/src/xmlrpc_array.o\
$(XMLRPC_DIR)/src/xmlrpc_authcookie.o\
$(XMLRPC_DIR)/src/xmlrpc_base64.o\
$(XMLRPC_DIR)/src/xmlrpc_build.o\
$(XMLRPC_DIR)/src/xmlrpc_data.o\
$(XMLRPC_DIR)/src/xmlrpc_datetime.o\
$(XMLRPC_DIR)/src/xmlrpc_decompose.o\
$(XMLRPC_DIR)/src/xmlrpc_expat.o\
$(XMLRPC_DIR)/src/xmlrpc_parse.o\
$(XMLRPC_DIR)/src/xmlrpc_serialize.o\
$(XMLRPC_DIR)/src/xmlrpc_server_abyss.o\
$(XMLRPC_DIR)/src/xmlrpc_server_cgi.o\
$(XMLRPC_DIR)/src/xmlrpc_string.o\
$(XMLRPC_DIR)/src/xmlrpc_struct.o\
$(XMLRPC_DIR)/lib/expat/xmltok/xmltok.o\
ws.o

LOCAL_CFLAGS = -w -I$(XMLRPC_DIR)/lib/expat/xmlparse -I$(XMLRPC_DIR)/lib/expat/xmltok -I$(XMLRPC_DIR) -I$(XMLRPC_DIR)/include
LOCAL_CFLAGS+= -I$(XMLRPC_DIR)/lib/abyss/src -I$(XMLRPC_DIR)/lib/util/include -D_THREAD -D__EXTENSIONS__
LOCAL_CFLAGS+= -I.

include $(BASE)/build/modmake.rules

$(XMLRPCLAS): $(XMLRPC_DIR) $(XMLRPC_DIR)/.update
	cd $(XMLRPC_DIR) && $(MAKE)
	$(TOUCH_TARGET)

$(XMLRPC_DIR)/lib/expat/xmltok/xmltok.o: $(XMLRPC_DIR)/lib/expat/xmltok/nametab.h

$(XMLRPC_DIR)/lib/expat/xmltok/nametab.h:
	cd $(XMLRPC_DIR)/lib/expat/gennmtab && $(MAKE)
	cd $(XMLRPC_DIR)/lib/expat/xmltok   && ../gennmtab/gennmtab > nametab.h

$(XMLRPC_DIR)/src/version.o: $(XMLRPC_DIR)/version.h

$(XMLRPC_DIR)/version.h:
	cd $(XMLRPC_DIR) && $(MAKE) version.h
	cd $(XMLRPC_DIR)/include && $(MAKE) xmlrpc-c/config.h

local_clean:
	cd $(XMLRPC_DIR) && $(MAKE) clean




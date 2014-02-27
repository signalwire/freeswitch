include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_xml_rpc

MAKEFLAGS=-j1
XMLRPC_DIR=$(switch_srcdir)/libs/xmlrpc-c

mod_LTLIBRARIES = mod_xml_rpc.la
mod_xml_rpc_la_SOURCES  = mod_xml_rpc.c
mod_xml_rpc_la_CFLAGS   = $(AM_CFLAGS) -I. -w -I$(XMLRPC_DIR)/lib/expat/xmlparse 
mod_xml_rpc_la_CFLAGS  += -I$(XMLRPC_DIR)/lib/expat/xmltok -I$(XMLRPC_DIR) -I$(XMLRPC_DIR)/include 
mod_xml_rpc_la_CFLAGS  += -I$(XMLRPC_DIR)/lib/abyss/src -I$(XMLRPC_DIR)/lib/util/include -D_THREAD -D__EXTENSIONS__ -D_GNU_SOURCE
mod_xml_rpc_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_xml_rpc_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

mod_xml_rpc_la_SOURCES += $(XMLRPC_DIR)/src/version.c\
$(XMLRPC_DIR)/lib/libutil/asprintf.c\
$(XMLRPC_DIR)/lib/util/casprintf.c\
$(XMLRPC_DIR)/lib/abyss/src/channel.c\
$(XMLRPC_DIR)/lib/abyss/src/chanswitch.c\
$(XMLRPC_DIR)/lib/util/cmdline_parser.c\
$(XMLRPC_DIR)/lib/abyss/src/conf.c\
$(XMLRPC_DIR)/lib/abyss/src/conn.c\
$(XMLRPC_DIR)/lib/abyss/src/data.c\
$(XMLRPC_DIR)/lib/abyss/src/date.c\
$(XMLRPC_DIR)/src/double.c\
$(XMLRPC_DIR)/lib/libutil/error.c\
$(XMLRPC_DIR)/lib/abyss/src/file.c\
$(XMLRPC_DIR)/lib/expat/gennmtab/gennmtab.c\
$(XMLRPC_DIR)/lib/util/getoptx.c\
$(XMLRPC_DIR)/lib/abyss/src/handler.c\
$(XMLRPC_DIR)/lib/abyss/src/http.c\
$(XMLRPC_DIR)/lib/abyss/src/init.c\
$(XMLRPC_DIR)/lib/libutil/make_printable.c\
$(XMLRPC_DIR)/lib/libutil/memblock.c\
$(XMLRPC_DIR)/src/method.c\
$(XMLRPC_DIR)/src/parse_value.c\
$(XMLRPC_DIR)/src/parse_datetime.c\
$(XMLRPC_DIR)/src/registry.c\
$(XMLRPC_DIR)/src/resource.c\
$(XMLRPC_DIR)/lib/abyss/src/response.c\
$(XMLRPC_DIR)/lib/libutil/select.c\
$(XMLRPC_DIR)/lib/abyss/src/server.c\
$(XMLRPC_DIR)/lib/abyss/src/session.c\
$(XMLRPC_DIR)/lib/libutil/sleep.c\
$(XMLRPC_DIR)/lib/abyss/src/socket.c\
$(XMLRPC_DIR)/lib/abyss/src/socket_unix.c\
$(XMLRPC_DIR)/lib/util/string_parser.c\
$(XMLRPC_DIR)/lib/util/stripcaseeq.c\
$(XMLRPC_DIR)/src/system_method.c\
$(XMLRPC_DIR)/lib/abyss/src/thread_pthread.c\
$(XMLRPC_DIR)/lib/libutil/time.c\
$(XMLRPC_DIR)/lib/abyss/src/token.c\
$(XMLRPC_DIR)/src/xml_trace.c\
$(XMLRPC_DIR)/lib/abyss/src/trace.c\
$(XMLRPC_DIR)/lib/libutil/utf8.c\
$(XMLRPC_DIR)/lib/libutil/string_number.c\
$(XMLRPC_DIR)/lib/libutil/base64.c\
$(XMLRPC_DIR)/lib/expat/xmlparse/xmlparse.c\
$(XMLRPC_DIR)/lib/expat/xmltok/xmlrole.c\
$(XMLRPC_DIR)/src/xmlrpc_array.c\
$(XMLRPC_DIR)/src/xmlrpc_authcookie.c\
$(XMLRPC_DIR)/src/xmlrpc_base64.c\
$(XMLRPC_DIR)/src/xmlrpc_build.c\
$(XMLRPC_DIR)/src/xmlrpc_data.c\
$(XMLRPC_DIR)/src/xmlrpc_datetime.c\
$(XMLRPC_DIR)/src/xmlrpc_decompose.c\
$(XMLRPC_DIR)/src/xmlrpc_expat.c\
$(XMLRPC_DIR)/src/xmlrpc_parse.c\
$(XMLRPC_DIR)/src/xmlrpc_serialize.c\
$(XMLRPC_DIR)/src/xmlrpc_server_abyss.c\
$(XMLRPC_DIR)/src/xmlrpc_server_cgi.c\
$(XMLRPC_DIR)/src/xmlrpc_string.c\
$(XMLRPC_DIR)/src/xmlrpc_struct.c\
$(XMLRPC_DIR)/lib/expat/xmltok/xmltok.c\
ws.c

BUILT_SOURCES=$(XMLRPC_DIR)/version.h $(XMLRPC_DIR)/lib/expat/xmltok/nametab.h
$(XMLRPC_DIR)/lib/expat/xmltok/xmltok.o: $(XMLRPC_DIR)/lib/expat/xmltok/nametab.h

$(XMLRPC_DIR)/lib/expat/xmltok/nametab.h:
	cd $(XMLRPC_DIR)/lib/expat/gennmtab && $(MAKE)
	cd $(XMLRPC_DIR)/lib/expat/xmltok   && ../gennmtab/gennmtab > nametab.h

$(XMLRPC_DIR)/src/version.o: $(XMLRPC_DIR)/version.h

$(XMLRPC_DIR)/version.h:
	cd $(XMLRPC_DIR) && $(MAKE) version.h
	cd $(XMLRPC_DIR)/include && $(MAKE) xmlrpc-c/config.h

clean:
	cd $(XMLRPC_DIR) && $(MAKE) clean





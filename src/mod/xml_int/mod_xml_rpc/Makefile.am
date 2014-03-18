include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_xml_rpc

mod_LTLIBRARIES = mod_xml_rpc.la
mod_xml_rpc_la_SOURCES  = mod_xml_rpc.c ws.c
mod_xml_rpc_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_xml_rpc_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

if SYSTEM_XMLRPCC
mod_xml_rpc_la_CFLAGS=$(SYS_XMLRPC_CFLAGS)
mod_xml_rpc_la_LDFLAGS+=$(SYS_XMLRPC_LDFLAGS)
else
MAKEFLAGS=-j1
XMLRPC_DIR=$(switch_srcdir)/libs/xmlrpc-c
XMLRPC_BUILDDIR=$(switch_builddir)/libs/xmlrpc-c
mod_xml_rpc_la_CFLAGS  = -I$(XMLRPC_DIR)/lib/expat/xmlparse -I$(XMLRPC_DIR)/lib/expat/xmltok -I$(XMLRPC_DIR) -I$(XMLRPC_DIR)/include
mod_xml_rpc_la_CFLAGS  += -I$(XMLRPC_DIR)/lib/abyss/src -I$(XMLRPC_DIR)/lib/util/include -D_THREAD -D__EXTENSIONS__ -D_GNU_SOURCE
mod_xml_rpc_la_CFLAGS  += -I$(XMLRPC_BUILDDIR)/include 
mod_xml_rpc_la_SOURCES += ../../../../libs/xmlrpc-c/src/version.c\
../../../../libs/xmlrpc-c/lib/libutil/asprintf.c\
../../../../libs/xmlrpc-c/lib/util/casprintf.c\
../../../../libs/xmlrpc-c/lib/abyss/src/channel.c\
../../../../libs/xmlrpc-c/lib/abyss/src/chanswitch.c\
../../../../libs/xmlrpc-c/lib/util/cmdline_parser.c\
../../../../libs/xmlrpc-c/lib/abyss/src/conf.c\
../../../../libs/xmlrpc-c/lib/abyss/src/conn.c\
../../../../libs/xmlrpc-c/lib/abyss/src/data.c\
../../../../libs/xmlrpc-c/lib/abyss/src/date.c\
../../../../libs/xmlrpc-c/src/double.c\
../../../../libs/xmlrpc-c/lib/libutil/error.c\
../../../../libs/xmlrpc-c/lib/abyss/src/file.c\
../../../../libs/xmlrpc-c/lib/expat/gennmtab/gennmtab.c\
../../../../libs/xmlrpc-c/lib/util/getoptx.c\
../../../../libs/xmlrpc-c/lib/abyss/src/handler.c\
../../../../libs/xmlrpc-c/lib/abyss/src/http.c\
../../../../libs/xmlrpc-c/lib/abyss/src/init.c\
../../../../libs/xmlrpc-c/lib/libutil/make_printable.c\
../../../../libs/xmlrpc-c/lib/libutil/memblock.c\
../../../../libs/xmlrpc-c/src/method.c\
../../../../libs/xmlrpc-c/src/parse_value.c\
../../../../libs/xmlrpc-c/src/parse_datetime.c\
../../../../libs/xmlrpc-c/src/registry.c\
../../../../libs/xmlrpc-c/src/resource.c\
../../../../libs/xmlrpc-c/lib/abyss/src/response.c\
../../../../libs/xmlrpc-c/lib/libutil/select.c\
../../../../libs/xmlrpc-c/lib/abyss/src/server.c\
../../../../libs/xmlrpc-c/lib/abyss/src/session.c\
../../../../libs/xmlrpc-c/lib/libutil/sleep.c\
../../../../libs/xmlrpc-c/lib/abyss/src/socket.c\
../../../../libs/xmlrpc-c/lib/abyss/src/socket_unix.c\
../../../../libs/xmlrpc-c/lib/util/string_parser.c\
../../../../libs/xmlrpc-c/lib/util/stripcaseeq.c\
../../../../libs/xmlrpc-c/src/system_method.c\
../../../../libs/xmlrpc-c/lib/abyss/src/thread_pthread.c\
../../../../libs/xmlrpc-c/lib/libutil/time.c\
../../../../libs/xmlrpc-c/lib/abyss/src/token.c\
../../../../libs/xmlrpc-c/src/xml_trace.c\
../../../../libs/xmlrpc-c/lib/abyss/src/trace.c\
../../../../libs/xmlrpc-c/lib/libutil/utf8.c\
../../../../libs/xmlrpc-c/lib/libutil/string_number.c\
../../../../libs/xmlrpc-c/lib/libutil/base64.c\
../../../../libs/xmlrpc-c/lib/expat/xmlparse/xmlparse.c\
../../../../libs/xmlrpc-c/lib/expat/xmltok/xmlrole.c\
../../../../libs/xmlrpc-c/src/xmlrpc_array.c\
../../../../libs/xmlrpc-c/src/xmlrpc_authcookie.c\
../../../../libs/xmlrpc-c/src/xmlrpc_base64.c\
../../../../libs/xmlrpc-c/src/xmlrpc_build.c\
../../../../libs/xmlrpc-c/src/xmlrpc_data.c\
../../../../libs/xmlrpc-c/src/xmlrpc_datetime.c\
../../../../libs/xmlrpc-c/src/xmlrpc_decompose.c\
../../../../libs/xmlrpc-c/src/xmlrpc_expat.c\
../../../../libs/xmlrpc-c/src/xmlrpc_parse.c\
../../../../libs/xmlrpc-c/src/xmlrpc_serialize.c\
../../../../libs/xmlrpc-c/src/xmlrpc_server_abyss.c\
../../../../libs/xmlrpc-c/src/xmlrpc_server_cgi.c\
../../../../libs/xmlrpc-c/src/xmlrpc_string.c\
../../../../libs/xmlrpc-c/src/xmlrpc_struct.c\
../../../../libs/xmlrpc-c/lib/expat/xmltok/xmltok.c
endif

mod_xml_rpc_la_CFLAGS  += $(AM_CFLAGS) -I. -w


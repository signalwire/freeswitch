ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/..
endif
SUBDIR = src
BUILDDIR = $(SRCDIR)
VPATH = .:$(SRCDIR)

include $(BUILDDIR)/Makefile.config

default: all

SUBDIRS =

ifeq ($(ENABLE_CPLUSPLUS),yes)
  SUBDIRS += cpp
endif

WININET_TRANSPORT_DIR = $(SRCDIR)/lib/wininet_transport
CURL_TRANSPORT_DIR    = $(SRCDIR)/lib/curl_transport
LIBWWW_TRANSPORT_DIR  = $(SRCDIR)/lib/libwww_transport

CASPRINTF_OBJ = $(BUILDDIR)/lib/util/casprintf.lo

TRANSPORT_OBJS =
ifeq ($(MUST_BUILD_WININET_CLIENT),yes)
  TRANSPORT_OBJS += $(WININET_TRANSPORT_DIR)/xmlrpc_wininet_transport.lo
endif
ifeq ($(MUST_BUILD_CURL_CLIENT),yes)
  TRANSPORT_OBJS += $(CURL_TRANSPORT_DIR)/xmlrpc_curl_transport.lo
endif
ifeq ($(MUST_BUILD_LIBWWW_CLIENT),yes)
  TRANSPORT_OBJS += $(LIBWWW_TRANSPORT_DIR)/xmlrpc_libwww_transport.lo
endif

ifeq ($(ENABLE_LIBXML2_BACKEND),yes)
  LIBXML_INCLUDES = $(LIBXML2_CFLAGS)
  LIBXML = $(LIBXML2_LIBS)
else
  LIBXML_INCLUDES = -I$(SRCDIR)/lib/expat/xmlparse
  LIBXML = $(BUILDDIR)/lib/expat/xmlparse/libxmlrpc_xmlparse.la \
           $(BUILDDIR)/lib/expat/xmltok/libxmlrpc_xmltok.la
endif

ABYSS_INCLUDES = -I$(SRCDIR)/lib/abyss/src

LIBXMLRPC_CLIENT_OBJS = xmlrpc_client.lo
LIBXMLRPC_SERVER_OBJS = xmlrpc_registry.lo
LIBXMLRPC_SERVER_ABYSS_OBJS = xmlrpc_server_abyss.lo

ifeq ($(ENABLE_LIBXML2_BACKEND),yes)
  XMLRPC_XML_PARSER = xmlrpc_libxml2.lo
else
  XMLRPC_XML_PARSER = xmlrpc_expat.lo
endif

LIBXMLRPC_OBJS = xmlrpc_support.lo xmlrpc_data.lo xmlrpc_builddecomp.lo \
		 xmlrpc_datetime.lo xmlrpc_array.lo xmlrpc_struct.lo \
	         $(XMLRPC_XML_PARSER) xmlrpc_parse.lo \
                 xmlrpc_serialize.lo xmlrpc_base64.lo \
		 xmlrpc_utf8.lo xmlrpc_authcookie.lo xmlrpc_strutil.lo

LIB_OBJS = $(LIBXMLRPC_CLIENT_OBJS) $(LIBXMLRPC_SERVER_OBJS)

TRANSPORT_INCLUDES = \
  -I$(WININET_TRANSPORT_DIR) \
  -I$(CURL_TRANSPORT_DIR) \
  -I$(LIBWWW_TRANSPORT_DIR) \

INCLUDES = -I$(BUILDDIR) -I$(SRCDIR) \
           -I$(SRCDIR)/include -I$(SRCDIR)/lib/util/include \
	   -I$(SRCDIR)/lib/abyss/src \
	   $(TRANSPORT_INCLUDES) \
	   $(LIBXML_INCLUDES) \

TARGET_LTLIBRARIES = libxmlrpc.la libxmlrpc_server.la \
     libxmlrpc_server_abyss.la

ifeq ($(MUST_BUILD_CLIENT),yes)
  TARGET_LTLIBRARIES += libxmlrpc_client.la
endif

ifeq ($(ENABLE_CGI_SERVER),yes)
  TARGET_LTLIBRARIES += libxmlrpc_server_cgi.la
endif

all: $(TARGET_LTLIBRARIES) $(SUBDIRS:%=%/all)

LDFLAGS = $(LADD)

LIBLDFLAGS = $(LDFLAGS_VERSINFO) -rpath $(LIBINST_DIR) $(LADD)

libxmlrpc.la: $(LIBXMLRPC_OBJS)
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LIBLDFLAGS) $^

libxmlrpc_client.la: xmlrpc_client.lo $(TRANSPORT_OBJS) $(CASPRINTF_OBJ)
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LIBLDFLAGS) $^

libxmlrpc_server.la: xmlrpc_registry.lo
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LIBLDFLAGS) $^

libxmlrpc_server_abyss.la: xmlrpc_server_abyss.lo
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LIBLDFLAGS) $^

libxmlrpc_server_cgi.la: xmlrpc_server_cgi.lo
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LIBLDFLAGS) $^

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

$(LIB_OBJS):%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) $(CFLAGS) $<

$(LIBXMLRPC_OBJS):%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) $(LIBXML_INCLUDES) \
	  $(CFLAGS) $<

$(LIBXMLRPC_SERVER_ABYSS_OBJS):%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) $(ABYSS_INCLUDES) \
	  $(CFLAGS) $<

xmlrpc_server_cgi.lo:%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) \
	  $(CFLAGS) $<

LTLIBRARIES_TO_INSTALL = $(TARGET_LTLIBRARIES)

check:
	$(MAKE) -C test check

.PHONY: install
install: install-common $(SUBDIRS:%=%/install)

.PHONY: clean clean-local distclean
clean: clean-common clean-local
clean-local: cpp/clean

distclean: clean distclean-common

.PHONY: dep
dep: $(SUBDIRS:%=%/dep) $(BUILDDIR)/transport_config.h dep-common

include $(SRCDIR)/Makefile.common

xmlrpc_client.lo: $(BUILDDIR)/transport_config.h

include Makefile.depend

ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
endif
SUBDIR = src/cpp
BUILDDIR = $(SRCDIR)
VPATH = .:$(SRCDIR)

include $(BUILDDIR)/Makefile.config

default: all

WININET_TRANSPORT_DIR = $(SRCDIR)/lib/wininet_transport
CURL_TRANSPORT_DIR    = $(SRCDIR)/lib/curl_transport
LIBWWW_TRANSPORT_DIR  = $(SRCDIR)/lib/libwww_transport

ifeq ($(ENABLE_LIBXML2_BACKEND),yes)
  LIBXML_INCLUDES = $(LIBXML2_CFLAGS)
  LIBXML = $(LIBXML2_LIBS)
else
  LIBXML_INCLUDES = -I$(SRCDIR)/lib/expat/xmlparse
  LIBXML = $(BUILDDIR)/lib/expat/xmlparse/libxmlrpc_xmlparse.la \
           $(BUILDDIR)/lib/expat/xmltok/libxmlrpc_xmltok.la
endif

ifeq ($(ENABLE_LIBXML2),yes)
  XMLRPC_XML_PARSER = xmlrpc_libxml2.lo
else
  XMLRPC_XML_PARSER = xmlrpc_expat.lo
endif

LIBXMLRPC = $(BUILDDIR)/src/libxmlrpc.la
LIBXMLRPC_SERVER = $(BUILDDIR)/src/libxmlrpc_server.la
LIBXMLRPC_CLIENT = $(BUILDDIR)/src/libxmlrpc_client.la

LIBXMLRPCPP_OBJS = girmem.o value.o fault.o outcome.o param_list.o xml.o
LIBXMLRPC_SERVERPP_OBJS = registry.o
LIBXMLRPC_SERVER_ABYSSPP_OBJS = server_abyss.o
LIBXMLRPC_CLIENTPP_OBJS = client.o client_simple.o

$(LIBXMLRPC_CLIENTPP_OBJS): $(BUILDDIR)/transport_config.h

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

TRANSPORT_INCLUDES = \
  -I$(WININET_TRANSPORT_DIR) \
  -I$(CURL_TRANSPORT_DIR) \
  -I$(LIBWWW_TRANSPORT_DIR) \

BASIC_INCLUDES = -I$(SRCDIR)/include -I$(BUILDDIR) -I$(SRCDIR) \
  -I$(SRCDIR)/lib/util/include

# We always statically-link our C++ code because it's tiny, and C++ is
# no good at shared libraries, anyway.  It's actually getting bigger,
# but since I'm determined not to expand the use of libtool (because
# it obfuscates the build process), I haven't figured out yet how to
# get the shared libraries to build for all the platforms.

# libxmlrpc_cpp.a is the legacy C++ wrapper library.  The others are the
# more elaborate replacements.

TARGET_LIBRARIES = \
  libxmlrpc_cpp.a \
  libxmlrpc++.a \
  libxmlrpc_server++.a \
  libxmlrpc_server_abyss++.a \
  libxmlrpc_client++.a \

TARGET_PROGS = cpptest

all: $(TARGET_LIBRARIES)

ifeq ($(MUST_BUILD_CURL_CLIENT),yes)
  LDFLAGS += $(shell curl-config --libs) -lpthread
endif
ifeq ($(MUST_BUILD_LIBWWW_CLIENT),yes)
  LDFLAGS += $(shell libwww-config --libs)
endif

LDFLAGS += $(LADD)

cpptest:%:%.o libxmlrpc_server_abyss++.a libxmlrpc_server++.a \
  libxmlrpc_client++.a libxmlrpc++.a libxmlrpc_cpp.a \
  $(LIBXMLRPC) $(LIBXMLRPC_SERVER) $(LIBXMLRPC_CLIENT) $(LIBXML)
	$(LIBTOOL) --mode=link $(CXXLD) -o $@ $(LDFLAGS) $^

LIBLDFLAGS = $(LADD)

libxmlrpc_cpp.a: XmlRpcCpp.o
	-rm -f $@
	$(AR) cru $@ $^
	$(RANLIB) $@

libxmlrpc++.a: $(LIBXMLRPCPP_OBJS)
	-rm -f $@
	$(AR) cru $@ $^
	$(RANLIB) $@

libxmlrpc_server++.a: $(LIBXMLRPC_SERVERPP_OBJS)
	-rm -f $@
	$(AR) cru $@ $^
	$(RANLIB) $@

libxmlrpc_server_abyss++.a: $(LIBXMLRPC_SERVER_ABYSSPP_OBJS)
	-rm -f $@
	$(AR) cru $@ $^
	$(RANLIB) $@

libxmlrpc_client++.a: $(LIBXMLRPC_CLIENTPP_OBJS)
	-rm -f $@
	$(AR) cru $@ $^
	$(RANLIB) $@

CXXFLAGS = $(CXXFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

cpptest.o:%.o:%.cpp
	$(CXX) -c $(BASIC_INCLUDES) $(CXXFLAGS) $<

XmlRpcCpp.o:%.o:%.cpp
	$(CXX) -c $(BASIC_INCLUDES) $(CXXFLAGS) $<

$(LIBXMLRPCPP_OBJS):%.o:%.cpp
	$(CXX) -c $(BASIC_INCLUDES) $(CXXFLAGS) $<

SERVER_INCLUDES = $(BASIC_INCLUDES) $(LIBXML_INCLUDES)

$(LIBXMLRPC_SERVERPP_OBJS):%.o:%.cpp
	$(CXX) -c $(SERVER_INCLUDES) $(CXXFLAGS) $<

SERVER_ABYSS_INCLUDES = $(SERVER_INCLUDES) -I$(SRCDIR)/lib/abyss/src

$(LIBXMLRPC_SERVER_ABYSSPP_OBJS):%.o:%.cpp
	$(CXX) -c $(SERVER_ABYSS_INCLUDES) $(CXXFLAGS) $<

CLIENT_INCLUDES = $(BASIC_INCLUDES) $(LIBXML_INCLUDES) $(TRANSPORT_INCLUDES)

$(LIBXMLRPC_CLIENTPP_OBJS):%.o:%.cpp
	$(CXX) -c $(CLIENT_INCLUDES) $(CXXFLAGS) $<



LIBRARIES_TO_INSTALL = $(TARGET_LIBRARIES)

check: cpptest
	for tst in $^; do \
	  ./$$tst; \
	done;

.PHONY: install
install: install-common

.PHONY: clean clean-local distclean
clean: clean-common clean-local
clean-local:
	rm -f $(TARGET_PROGS)

distclean: clean distclean-common

# INCLUDES and DEP_SOURCES are used by dep-common target
INCLUDES = $(BASIC_INCLUDES) $(CLIENT_INCLUDES) $(LIBXML_INCLUDES) \
  $(SERVER_INCLUDES) $(SERVER_ABYSS_INCLUDES) $(TRANSPORT_INCLUDES)
DEP_SOURCES = *.cpp

.PHONY: dep
dep: $(BUILDDIR)/transport_config.h dep-common

include $(SRCDIR)/Makefile.common

include Makefile.depend

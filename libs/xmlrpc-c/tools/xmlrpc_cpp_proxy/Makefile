ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  TOOLSDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(TOOLSDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := tools/xmlrpc_cpp_proxy

default: all

include $(BLDDIR)/config.mk

PROGRAMS_TO_INSTALL = xmlrpc_cpp_proxy

include $(SRCDIR)/tools/common.mk

INCLUDES = -I$(BLDDIR) -I$(BLDDIR)/include -I$(SRCDIR)/include

CXXFLAGS = $(INCLUDES) $(CXXFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

LDFLAGS = $(LADD)

all: xmlrpc_cpp_proxy

OBJECTS = \
  xmlrpc_cpp_proxy.o \
  xmlrpcMethod.o \
  xmlrpcType.o \
  proxyClass.o \
  systemProxy.o \

LIBS = \
  $(LIBXMLRPC_CLIENTPP) \
  $(LIBXMLRPCPP) \
  $(LIBXMLRPC_CLIENT) \
  $(LIBXMLRPC) \
  $(LIBXMLRPC_XML) \
  $(LIBXMLRPC_UTIL) \

LDLIBS = $(CLIENTPP_LDLIBS) $(CLIENT_LDLIBS)

xmlrpc_cpp_proxy:  $(OBJECTS) $(LIBS)
	$(CXXLD) -o $@ $(LDFLAGS) $(OBJECTS) $(LDLIBS) $(LADD)

%.o:%.cpp $(BLDDIR)/include/xmlrpc-c/config.h
	$(CXX) -c $(CXXFLAGS) $<

# This common.mk dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/tools/common.mk: srcdir blddir

include Makefile.depend

.PHONY: clean
clean: clean-common
	rm -f xmlrpc_cpp_proxy

.PHONY: distclean
distclean: clean distclean-common

.PHONY: dep
dep: dep-common

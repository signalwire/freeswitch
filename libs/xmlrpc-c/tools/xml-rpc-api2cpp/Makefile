ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  TOOLSDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(TOOLSDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := tools/xml-rpc-api2cpp

default: all

include $(BLDDIR)/config.mk

include $(SRCDIR)/tools/common.mk

INCLUDES = -I$(BLDDIR) -I$(BLDDIR)/include -I$(SRCDIR)/include

CXXFLAGS = $(INCLUDES) $(CXXFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

LDFLAGS = $(LADD)

LDLIBS = -L$(BLDDIR)/src/cpp -lxmlrpc_cpp -lxmlrpc_server $(CLIENT_LDLIBS)

PROGS :=

ifeq ($(MUST_BUILD_CLIENT),yes)
  PROGS += xml-rpc-api2cpp
endif

all: $(PROGS)

OBJECTS = \
  xml-rpc-api2cpp.o \
  DataType.o \
  XmlRpcFunction.o \
  XmlRpcClass.o \
  SystemProxy.o \

xml-rpc-api2cpp: \
  $(OBJECTS) \
  $(LIBXMLRPC_CPP) \
  $(LIBXMLRPC_CLIENT) \
  $(LIBXMLRPC_SERVER) \
  $(LIBXMLRPC) \
  $(LIBXMLRPC_XML) \
  $(LIBXMLRPC_UTIL)
	$(CXXLD) -o $@ $(LDFLAGS) $(OBJECTS) $(LDLIBS)

%.o:%.cpp
	$(CXX) -c $(CXXFLAGS) $<

# This common.mk dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir blddir

include Makefile.depend

.PHONY: clean
clean: clean-common
	rm -f $(PROGS)

.PHONY: distclean
distclean: clean distclean-common

.PHONY: dep
dep: dep-common

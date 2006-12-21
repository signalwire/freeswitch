ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
BUILDDIR= $(SRCDIR)
endif

default: all

include $(BUILDDIR)/Makefile.config

include ../Makefile.common

INCLUDES = -I$(SRCDIR) -I$(SRCDIR)/include

CXXFLAGS = $(INCLUDES) $(CXXFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

LDFLAGS = $(CLIENT_LDFLAGS) $(LADD)

all: xml-rpc-api2cpp

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
  $(LIBXML)
	$(LIBTOOL) --mode=link $(CXXLD) -o $@ $(LDFLAGS) $^

%.o:%.cpp
	$(LIBTOOL) --mode=compile $(CXX) -c $(CXXFLAGS) $<

include Makefile.depend

.PHONY: clean
clean: clean-common
	rm -f xml-rpc-api2cpp

.PHONY: distclean
distclean: clean distclean-common

.PHONY: dep
dep: dep-common

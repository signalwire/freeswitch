ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  TOOLSDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(TOOLSDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := tools/xml-rpc-api2cpp

default: all

include $(BLDDIR)/config.mk

PROGS := xml-rpc-api2cpp
PROGRAMS_TO_INSTALL := $(PROGS)
MAN_FILES_TO_INSTALL := xml-rpc-api2cpp.1

all: $(PROGS)

include $(SRCDIR)/tools/common.mk

# <sstream.h> in Glibc 2.2 has a bug that results in inlining failure,
# so we disable warnings for that:
CFLAGS_LOCAL = -Wno-inline

INCLUDES = -I$(BLDDIR) -I$(BLDDIR)/include -Isrcdir/include

LDFLAGS = $(LADD)

LDLIBS = -L$(BLDDIR)/src/cpp -lxmlrpc_cpp -L$(BLDDIR)/src -lxmlrpc_server \
  $(CLIENT_LDLIBS)

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
	$(CXX) -c $(CXXFLAGS_ALL) $<

# This common.mk dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/tools/common.mk: srcdir blddir

include depend.mk

.PHONY: clean
clean: clean-common
	rm -f $(PROGS)

.PHONY: distclean
distclean: clean distclean-common

.PHONY: dep
dep: dep-common

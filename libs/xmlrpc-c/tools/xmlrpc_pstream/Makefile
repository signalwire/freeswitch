ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  TOOLSDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(TOOLSDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := tools/xmlrpc_pstream

READLINE_LDLIBS = -lreadline -lncurses

default: all

include $(BLDDIR)/config.mk

PROGRAMS_TO_INSTALL = xmlrpc_pstream

include $(SRCDIR)/tools/common.mk

INCLUDES = \
  -I../lib/include \
  -Isrcdir/lib/util/include \
  -Iblddir \
  -Iblddir/include \
  -Isrcdir/include \

LDFLAGS = $(LADD)

all: xmlrpc_pstream

OBJECTS = \
  xmlrpc_pstream.o \
  blddir/tools/lib/dumpvalue.o \

LIBS = \
  $(LIBXMLRPC_CLIENTPP) \
  $(LIBXMLRPCPP) \
  $(LIBXMLRPC_CLIENT) \
  $(LIBXMLRPC) \
  $(LIBXMLRPC_XML) \
  $(LIBXMLRPC_UTIL) \

LDLIBS = $(CLIENTPP_LDLIBS) $(CLIENT_LDLIBS) $(READLINE_LDLIBS)

UTIL_OBJS = \
  casprintf.o \
  cmdline_parser_cpp.o \
  cmdline_parser.o \
  getoptx.o \
  string_parser.o \
  stripcaseeq.o \

UTILS = $(UTIL_OBJS:%=$(UTIL_DIR)/%)

xmlrpc_pstream:  $(OBJECTS) $(LIBS) $(UTILS)
	$(CXXLD) -o $@ $(LDFLAGS) $(OBJECTS) $(UTILS) $(LDLIBS) $(LADD)

%.o:%.cpp blddir/include/xmlrpc-c/config.h
	$(CXX) -c $(CXXFLAGS_ALL) $<

# This common.mk dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/tools/common.mk: srcdir blddir

include depend.mk

.PHONY: clean
clean: clean-common
	rm -f xmlrpc_pstream

.PHONY: distclean
distclean: clean distclean-common

.PHONY: dep
dep: dep-common

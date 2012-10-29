ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  TOOLSDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(TOOLSDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := tools/xmlrpc

default: all

include $(BLDDIR)/config.mk

PROGRAMS_TO_INSTALL = xmlrpc

include $(SRCDIR)/tools/common.mk

INCLUDES = \
  -I../lib/include \
  -Iblddir \
  -Iblddir/include \
  -Isrcdir/include \
  -Isrcdir/lib/util/include

LDFLAGS = $(LADD)

all: xmlrpc

UTIL_OBJS = \
  casprintf.o \
  cmdline_parser.o \
  getoptx.o \
  stripcaseeq.o \
  string_parser.o \

UTILS = $(UTIL_OBJS:%=$(UTIL_DIR)/%)

DUMPVALUE = blddir/tools/lib/dumpvalue.o

# Can we just use $(LIBS) in the link?

LIBS = $(LIBXMLRPC_CLIENT) $(LIBXMLRPC) $(LIBXMLRPC_XML) $(LIBXMLRPC_UTIL)

XMLRPC_OBJS = xmlrpc.o $(DUMPVALUE)

xmlrpc: $(XMLRPC_OBJS) $(LIBS) $(UTILS)
	$(CCLD) -o $@ $(LDFLAGS) $(XMLRPC_OBJS) $(UTILS) $(CLIENT_LDLIBS)

%.o:%.c $(BLDDIR)/include/xmlrpc-c/config.h
	$(CC) -c $(CFLAGS_ALL) $<

# This common.mk dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/tools/common.mk: srcdir blddir

include depend.mk

.PHONY: dep
dep: dep-common

.PHONY: clean
clean: clean-common
	rm -f xmlrpc

.PHONY: distclean
distclean: clean distclean-common

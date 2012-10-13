ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  TOOLSDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(TOOLSDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := tools/xmlrpc_transport

default: all

include $(BLDDIR)/config.mk

PROGRAMS_TO_INSTALL = xmlrpc_transport

include $(SRCDIR)/tools/common.mk

INCLUDES = \
  -I$(BLDDIR) \
  -I$(BLDDIR)/include \
  -Isrcdir/include \
  -Isrcdir/lib/util/include

LDFLAGS = $(LADD)

all: xmlrpc_transport

UTIL_OBJS = \
  casprintf.o \
  cmdline_parser.o \
  getoptx.o \
  stripcaseeq.o \
  string_parser.o \

UTILS = $(UTIL_OBJS:%=$(UTIL_DIR)/%)

LIBS = $(LIBXMLRPC_CLIENT) $(LIBXMLRPC) $(LIBXMLRPC_XML) $(LIBXMLRPC_UTIL)

xmlrpc_transport:%:%.o $(LIBS) $(UTILS)
	$(CCLD) -o $@ $(LDFLAGS) $< $(CLIENT_LDLIBS) $(UTILS)

%.o:%.c
	$(CC) -c $(CFLAGS_ALL) $<

# This common.mk dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/tools/common.mk: srcdir blddir

include depend.mk

.PHONY: dep
dep: dep-common

.PHONY: clean
clean: clean-common
	rm -f xmlrpc_transport config.h

.PHONY: distclean
distclean: clean distclean-common

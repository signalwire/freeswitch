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
  -I$(SRCDIR)/include \
  -I$(SRCDIR)/lib/util/include

CFLAGS = $(CFLAGS_COMMON) $(INCLUDES) $(CFLAGS_PERSONAL) $(CADD)

LDFLAGS = $(LADD)

all: xmlrpc_transport

UTIL_OBJS = \
  casprintf.o \
  cmdline_parser.o \
  getoptx.o \
  stripcaseeq.o \
  string_parser.o \

UTILS = $(UTIL_OBJS:%=$(UTIL_DIR)/%)

# These are the Libtool .la files.  We use them only for make dependencies.
# We'd like to use these in the link rule, by using libtool --link, but
# Libtool adds -l options to the link to cover the dependencies that are
# recorded inside the shared libraries.  And it doesn't add the necessary
# -L options (it can't), so we end up with a messy mixture of the two
# forms of specifying libraries.

LIBS = $(LIBXMLRPC_CLIENT) $(LIBXMLRPC) $(LIBXMLRPC_XML) $(LIBXMLRPC_UTIL)

xmlrpc_transport:%:%.o $(LIBS) $(UTILS)
	$(CCLD) -o $@ $(LDFLAGS) $< $(CLIENT_LDLIBS) $(UTILS)

%.o:%.c
	$(CC) -c $(CFLAGS) $<

# This common.mk dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/tools/common.mk: srcdir blddir

include Makefile.depend

.PHONY: dep
dep: dep-common

.PHONY: clean
clean: clean-common
	rm -f xmlrpc_transport config.h

.PHONY: distclean
distclean: clean distclean-common

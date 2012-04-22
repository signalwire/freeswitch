###############################################################################
# This directory builds object modules that provide utility functions that
# programs can use.  Not libraries, though -- just programs.  The reason
# we don't want any library to use object modules in here is that they'll
# probably pollute the name space when users link those libraries to their
# programs.  In fact, if more than one Xmlrpc-c library includes one of these
# modules, the libraries will conflict with each other.
#
# So a utility function that is to be used by libraries (and, optionally,
# programs) should go in libxmlrpc_util.  libxmlrpc_util is a prerequisite
# for many Xmlrpc-c libraries, gets included in a program link only once,
# and uses external symbol names that have the "xmlrpc_" prefix to avoid
# collision with users' code.
#
# If we knew a portable way to link multiple object modules into one and
# restrict the symbols exported by the whole, we could avoid this mess and
# just link utility object modules into each Xmlrpc-c library.
##############################################################################

ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  LIBDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/util

default: all

include $(BLDDIR)/config.mk

OMIT_UTILS_RULE = Y

include $(SRCDIR)/common.mk

LIBOBJS = \
  casprintf.o \
  cmdline_parser.o \
  cmdline_parser_cpp.o \
  getoptx.o \
  string_parser.o \
  stripcaseeq.o \

.PHONY: all
all: $(LIBOBJS)

INCLUDES = -I$(SRCDIR)/$(SUBDIR)/include -I$(BLDDIR)

CFLAGS = $(CFLAGS_COMMON) $(INCLUDES) $(CFLAGS_PERSONAL) $(CADD)

%.o:%.c
	$(CC) -c $(CFLAGS) $<

%.o:%.cpp
	$(CXX) -c $(CFLAGS) $<

include Makefile.depend

.PHONY: clean distclean
clean: clean-common

distclean: clean distclean-common

.PHONY: dep
dep: dep-common

install:
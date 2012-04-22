###############################################################################
# This directory builds libxmlrpc_util, which contains utility
# functions that are used by the Xmlprc-c # libraries, and also
# directly by Xmlrpc-c programs.
#
# The functions in this library are characterized by being general purpose
# programming functions, such as one might wish were in the standard C
# library, which have nothing in particular to do with XML-RPC.
###############################################################################

ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  LIBDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/libutil

include $(BLDDIR)/config.mk

default: all

TARGET_LIBRARY_NAMES := libxmlrpc_util

STATIC_LIBRARIES_TO_INSTALL = libxmlrpc_util.a

SHARED_LIBS_TO_BUILD := libxmlrpc_util
SHARED_LIBS_TO_INSTALL := libxmlrpc_util

TARGET_MODS = \
  asprintf \
  error \
  make_printable \
  memblock \
  select \
  sleep \
  time \
  utf8 \

OMIT_LIBXMLRPC_UTIL_RULE=Y
MAJ=3
  # Major number of shared libraries in this directory

include $(SRCDIR)/common.mk

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

INCLUDES = -I$(BLDDIR) -Isrcdir \
           -I$(BLDDIR)/include -Isrcdir/include -Isrcdir/lib/util/include

UTIL_SHLIB = $(call shlibfn,libxmlrpc_util)
#UTIL_SHLIB is e.g. libxmlrpc_util.so.3.1
UTIL_SHLIBLE = $(call shliblefn,libxmlrpc_util)
#UTIL_SHLIBLE is e.g. libxmlrpc_util.so

ifneq ($(SHARED_LIB_TYPE),NONE)
  TARGET_SHARED_LIBS := $(UTIL_SHLIB) $(UTIL_SHLIBLE)
  endif

# This 'common.mk' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir blddir

.PHONY: all
all: libxmlrpc_util.a $(TARGET_SHARED_LIBS) $(TARGET_SHARED_LE_LIBS)

# Rule for this is in common.mk, courtesy of TARGET_LIBRARY_NAMES:
$(UTIL_SHLIB): $(TARGET_MODS:%=%.osh)
$(UTIL_SHLIB): LIBOBJECTS = $(TARGET_MODS:%=%.osh)

# Rule for this is in common.mk, courtesy of TARGET_LIBRARY_NAMES:

libxmlrpc_util.a: $(TARGET_MODS:%=%.o)
libxmlrpc_util.a: LIBOBJECTS = $(TARGET_MODS:%=%.o)

#-----------------------------------------------------------------------------
#  RULES TO COMPILE OBJECT MODULES FOR LIBRARIES
#-----------------------------------------------------------------------------

# Rules to compile object modules from which to build the static and shared
# library are in common.mk, courtesy of TARGET_MODS.

.PHONY: install
install: install-common

.PHONY: clean distclean
clean: clean-common

distclean: clean distclean-common

.PHONY: dep
dep: dep-common

include Makefile.depend

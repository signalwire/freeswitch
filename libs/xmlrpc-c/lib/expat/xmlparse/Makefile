###############################################################################
# This directory builds libxmlrpc_xmlparse, an XML parser.  This is
# essentially the separately distributed Expat library from 2001, but
# with slight changes.  The main reason it is bundled with Xmlrpc-c is
# to make the latter easier to build and use.
#
# The library is about XML in general.  There is nothing specific to
# XML-RPC here.
###############################################################################

ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  EXPATDIR := $(call updir,$(CURDIR))
  LIBDIR := $(call updir,$(EXPATDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/expat/xmlparse

XMLTOKDIR = srcdir/lib/expat/xmltok
UTILDIR = srcdir/lib/util

default: all

include $(BLDDIR)/config.mk

TARGET_LIBRARY_NAMES := libxmlrpc_xmlparse

STATIC_LIBRARIES_TO_INSTALL = libxmlrpc_xmlparse.a

SHARED_LIBS_TO_BUILD := libxmlrpc_xmlparse
SHARED_LIBS_TO_INSTALL := libxmlrpc_xmlparse

TARGET_MODS = xmlparse

OMIT_XMLPARSE_LIB_RULE=Y
MAJ=3
  # Major number of shared libraries in this directory

include $(SRCDIR)/common.mk

INCLUDES = \
  -I$(BLDDIR) \
  -I$(XMLTOKDIR) \
  -I$(UTILDIR)/include \
  -Isrcdir/include \

# LIBDEP is the shared libraries on which libxmlrpc_abyss depends.
# The runtime loader should load these libraries when it loads libxmlrpc_abyss.

LIBDEP = $(LIBXMLRPC_XMLTOK) $(LIBXMLRPC_UTIL)

XMLPARSE_SHLIB = $(call shlibfn,libxmlrpc_xmlparse)
#XMLPARSE_SHLIB is e.g. libxmlrpc_xmlparse.so.3.1
XMLPARSE_SHLIBLE = $(call shliblefn,libxmlrpc_xmlparse)
#XMLPARSE_SHLIBLE is e.g. libxmlrpc_xmlparse.so

.PHONY: all
all: libxmlrpc_xmlparse.a $(TARGET_SHARED_LIBRARIES) $(TARGET_SHARED_LE_LIBS)

# Rule for this is in common.mk, courtesy of TARGET_LIBRARY_NAMES:
$(XMLPARSE_SHLIB): $(TARGET_MODS:%=%.osh) $(LIBDEP)
$(XMLPARSE_SHLIB): LIBOBJECTS = $(TARGET_MODS:%=%.osh) $(LIBDEP)

# Rule for this is in common.mk, courtesy of TARGET_STATIC_LIBRARIES:

libxmlrpc_xmlparse.a: $(TARGET_MODS:%=%.o)
libxmlrpc_xmlparse.a: LIBOBJECTS = $(TARGET_MODS:%=%.o)

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

# This 'common.mk' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir

include depend.mk

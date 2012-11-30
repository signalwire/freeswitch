###############################################################################
# This directory builds libxmlrpc_xmltok, an XML token parser.  This is
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
SUBDIR := lib/expat/xmltok

include $(BLDDIR)/config.mk

# I can't figure out what XML_BYTE_ORDER is, but it doesn't look like the
# code has ever defined it.  That means it's treated like 0 in #if.  Since
# we started using the Gcc -Wundef option, that generates a warning, so
# se set it explicitly to 0 here.

CFLAGS_LOCAL = -DXML_BYTE_ORDER=0

# -I. is necessary when blddir != srcdir
INCLUDES = -I. -I$(BLDDIR) -Isrcdir/lib/util/include -Isrcdir/include

default: all

TARGET_LIBRARY_NAMES := libxmlrpc_xmltok

STATIC_LIBRARIES_TO_INSTALL = libxmlrpc_xmltok.a

SHARED_LIBS_TO_BUILD := libxmlrpc_xmltok
SHARED_LIBS_TO_INSTALL := libxmlrpc_xmltok

TARGET_MODS = xmltok xmlrole

OMIT_XMLTOK_LIB_RULE = Y
MAJ=3
  # Major number of shared libraries in this directory

include $(SRCDIR)/common.mk

# This 'common.mk' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir blddir

XMLTOK_SHLIB = $(call shlibfn,libxmlrpc_xmltok)
#XMLTOK_SHLIB is e.g. libxmlrpc_xmltok.so.3.1
XMLTOK_SHLIBLE = $(call shliblefn,libxmlrpc_xmltok)
#XMLTOK_SHLIBLE is e.g. libxmlrpc_xmltok.so

.PHONY: all
all: libxmlrpc_xmltok.a $(TARGET_SHARED_LIBRARIES) $(TARGET_SHARED_LE_LIBS)

# Rule for this is in common.mk, courtesy of TARGET_LIBRARY_NAMES
$(XMLTOK_SHLIB): $(TARGET_MODS:%=%.osh)
$(XMLTOK_SHLIB): LIBOBJECTS = $(TARGET_MODS:%=%.osh)

# Rule for this is in common.mk, courtesy of TARGET_STATIC_LIBRARIES:

libxmlrpc_xmltok.a: $(TARGET_MODS:%=%.o)
libxmlrpc_xmltok.a: LIBOBJECTS = $(TARGET_MODS:%=%.o)

#-----------------------------------------------------------------------------
#  RULES TO COMPILE OBJECT MODULES FOR LIBRARIES
#-----------------------------------------------------------------------------

# Rules to compile object modules from which to build the static and shared
# library are in common.mk, courtesy of TARGET_MODS.


.PHONY: clean
clean: clean-common
	rm -f nametab.h

.PHONY: distclean
distclean: clean distclean-common

.PHONY: tags
tags: TAGS

.PHONY: distdir
distdir:

.PHONY: install
install: install-common

.PHONY: dep
dep: dep-common

GENNMTAB = ../gennmtab/gennmtab

nametab.h: $(GENNMTAB)
	rm -f $@
	$(GENNMTAB) >$@

$(GENNMTAB):
	$(MAKE) -C $(dir $@) $(notdir $@)

xmltok.o xmltok.osh: nametab.h xmltok_impl.c

include depend.mk

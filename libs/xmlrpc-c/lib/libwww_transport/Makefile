ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  LIBDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/libwww_transport

include $(BLDDIR)/config.mk

default: all

.PHONY: all
all: xmlrpc_libwww_transport.o xmlrpc_libwww_transport.osh

# Rules for the above dependencies are in common.mk,
# courtesy of TARGET_MODS.

TARGET_MODS = xmlrpc_libwww_transport

OMIT_LIBWWW_TRANSPORT_RULE=Y

include $(SRCDIR)/common.mk

# This 'common.mk' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir blddir

LIBWWW_INCLUDES := $(shell libwww-config --cflags)

INCLUDES = \
  -I$(BLDDIR) \
  -I$(BLDDIR)/include \
  -Isrcdir/include \
  -Isrcdir/lib/util/include \
  $(LIBWWW_INCLUDES)

.PHONY: clean
clean: clean-common

.PHONY: distclean
distclean: clean distclean-common

.PHONY: tags
tags: TAGS

.PHONY: distdir
distdir:

.PHONY: install
install:

.PHONY: dep
dep: dep-common

include depend.mk

# Need this dependency for those who don't use depend.mk.
# Without it, version.h doesn't get created.
xmlrpc_libwww_transport.o xmlrpc_libwww_transport.osh: version.h

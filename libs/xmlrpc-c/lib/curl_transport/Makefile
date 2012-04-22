ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  LIBDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/curl_transport

include $(BLDDIR)/config.mk

default: all

.PHONY: all
all: xmlrpc_curl_transport.o xmlrpc_curl_transport.osh

# Rules for the above dependencies are in common.mk,
# courtesy of TARGET_MODS.

TARGET_MODS = xmlrpc_curl_transport

OMIT_CURL_TRANSPORT_RULE=Y

include $(SRCDIR)/common.mk

CURL_INCLUDES := $(shell curl-config --cflags)
# We expect that curl-config --cflags just gives us -I options, because
# we need just the -I options for 'make dep'.  Plus, it's scary to think
# of what any other compiler flag would do to our compile.

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

INCLUDES = \
  -I$(BLDDIR) \
  -I$(BLDDIR)/include \
  -I$(SRCDIR)/include \
  -I$(SRCDIR)/lib/util/include \
  $(CURL_INCLUDES)

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

include Makefile.depend

# Need this dependency for those who don't use Makefile.depend.
# Without it, version.h doesn't get created.
xmlrpc_curl_transport.o xmlrpc_curl_transport.osh: version.h

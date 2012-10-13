ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  SRCDIR := $(call updir,$(CURDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := tools

include $(BLDDIR)/config.mk

SUBDIRS = \
  binmode-rpc-kit \
  lib \
  turbocharger \

ifeq ($(MUST_BUILD_CLIENT),yes)
  SUBDIRS += xmlrpc xmlrpc_transport

  ifeq ($(ENABLE_CPLUSPLUS),yes)
    SUBDIRS += xml-rpc-api2cpp xml-rpc-api2txt xmlrpc_cpp_proxy

    ifeq ($(BUILD_XMLRPC_PSTREAM),yes)
      SUBDIRS += xmlrpc_pstream
    endif
  endif
endif

.PHONY: all clean distclean install check dep

all: $(SUBDIRS:%=%/all)

clean: $(SUBDIRS:%=%/clean)

distclean: $(SUBDIRS:%=%/distclean)

install: $(SUBDIRS:%=%/install)

check:

dep: $(SUBDIRS:%=%/dep)

include $(SRCDIR)/common.mk

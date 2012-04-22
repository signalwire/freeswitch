ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  SRCDIR := $(call updir,$(CURDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := tools

include $(BLDDIR)/config.mk

SUBDIRS = binmode-rpc-kit turbocharger

ifeq ($(MUST_BUILD_CLIENT),yes)
  SUBDIRS += xmlrpc xmlrpc_transport

  ifeq ($(ENABLE_CPLUSPLUS),yes)
    SUBDIRS += xml-rpc-api2cpp xmlrpc_cpp_proxy
    # We could add 'xmlrpc_pstream' here, but we don't because we don't
    # want to deal with finding the Readline/Ncurses libraries.
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

SUBDIR=tools
ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/..
BUILDDIR = $(SRCDIR)
endif

include $(SRCDIR)/Makefile.config

SUBDIRS = binmode-rpc-kit turbocharger

ifeq ($(MUST_BUILD_CLIENT),yes)
  SUBDIRS += xmlrpc xmlrpc_transport
endif

ifeq ($(ENABLE_CPLUSPLUS),yes)
  SUBDIRS += xml-rpc-api2cpp
endif

.PHONY: all clean distclean install dep

all: $(SUBDIRS:%=%/all)

clean: $(SUBDIRS:%=%/clean)

distclean: $(SUBDIRS:%=%/distclean)

install: $(SUBDIRS:%=%/install)

dep: $(SUBDIRS:%=%/dep)

include $(SRCDIR)/Makefile.common

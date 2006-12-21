# Since the programs in this directories are examples for the user, this
# make file should be as ordinary as possible.  It should not rely heavily
# on included make files or configuration parameters.  It should not use
# libtool.  Also, we don't try to build or rebuild the libraries on which
# these programs depend.


ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
BUILDDIR = $(SRCDIR)
endif

default: all

include $(BUILDDIR)/Makefile.config

CXXFLAGS = $(CXXFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)
LDFLAGS = $(LADD)

# If this were a real application, working from an installed copy of
# Xmlrpc-c, XMLRPC_C_CONFIG would just be 'xmlrpc-c-config'.  It would be
# found in the user's PATH.
XMLRPC_C_CONFIG = $(BUILDDIR)/xmlrpc-c-config.test


SERVERPROGS_ABYSS = \
  xmlrpc_sample_add_server \

LEGACY_CLIENTPROGS = \
  meerkat-app-list

CLIENTPROGS = \
  xmlrpc_sample_add_client \
  sample_add_client_complex \

# Build up PROGS:
PROGS = 

ifeq ($(ENABLE_ABYSS_SERVER),yes)
  PROGS += $(SERVERPROGS_ABYSS)
endif

ifeq ($(MUST_BUILD_CLIENT),yes)
  PROGS += $(CLIENTPROGS) $(LEGACY_CLIENTPROGS)
endif

INCLUDES = $(shell $(XMLRPC_C_CONFIG) c++2 client abyss-server --cflags)

LDADD_SERVER_ABYSS = \
  $(shell $(XMLRPC_C_CONFIG) c++2 abyss-server --ldadd)

LDADD_CLIENT = \
  $(shell $(XMLRPC_C_CONFIG) c++2 client --ldadd)

LDADD_BASE = \
  $(shell $(XMLRPC_C_CONFIG) c++2 --ldadd)

LDADD_LEGACY_CLIENT = \
  $(shell $(XMLRPC_C_CONFIG) c++ client --ldadd)

all: $(PROGS)

$(SERVERPROGS_ABYSS):%:%.o
	$(CXXLD) -o $@ $(LDFLAGS) $^ $(LDADD_SERVER_ABYSS)

$(LEGACY_CLIENTPROGS):%:%.o
	$(CXXLD) -o $@ $(LDFLAGS) $^ $(LDADD_LEGACY_CLIENT)

$(CLIENTPROGS):%:%.o
	$(CXXLD) -o $@ $(LDFLAGS) $^ $(LDADD_CLIENT)

%.o:%.cpp
	$(CXX) -c $(INCLUDES) $(CXXFLAGS) $<

*.c: config.h xmlrpc_amconfig.h

config.h:
	$(LN_S) $(BUILDDIR)/xmlrpc_config.h $@
xmlrpc_amconfig.h:
	$(LN_S) $(BUILDDIR)/$@ .

include $(SRCDIR)/Makefile.common

.PHONY: clean
clean: clean-common
	rm -f $(PROGS) config.h xmlrpc_amconfig.h

.PHONY: distclean
distclean: clean

.PHONY: dep depend
dep depend:
# We don't do dependencies in this directory, because it's supposed to be
# an example of what a program outside this package would do, so we can't
# go weaving it into the rest of the package.  Ergo, a developer must
# carefully clean and remake examples as he updates other parts of the tree.

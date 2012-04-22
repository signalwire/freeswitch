# Since the programs in this directories are examples for the user, this
# make file should be as ordinary as possible.  It should not rely heavily
# on included make files or configuration parameters.  It should not use
# libtool.  Also, we don't try to build or rebuild the libraries on which
# these programs depend.


ifeq ($(SRCDIR),)
SRCDIR = $(CURDIR)/../..
BLDDIR = $(SRCDIR)
endif
SUBDIR=examples/cpp

include $(BLDDIR)/config.mk

default: all

CXXFLAGS = $(CXXFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)
LDFLAGS = $(LADD)

# If this were a real application, working from an installed copy of
# Xmlrpc-c, XMLRPC_C_CONFIG would just be 'xmlrpc-c-config'.  It would be
# found in the user's PATH.
XMLRPC_C_CONFIG = $(BLDDIR)/xmlrpc-c-config.test


SERVERPROGS_ABYSS = \
  xmlrpc_inetd_server \
  xmlrpc_loop_server \
  xmlrpc_sample_add_server \

CLIENTPROGS = \
  xmlrpc_sample_add_client \
  sample_add_client_complex \
  asynch_client \

# Build up PROGS:
PROGS = 

ifeq ($(ENABLE_ABYSS_SERVER),yes)
  PROGS += $(SERVERPROGS_ABYSS)
endif

ifeq ($(MUST_BUILD_CLIENT),yes)
  PROGS += $(CLIENTPROGS)
endif

PROGS += pstream_inetd_server

ifeq ($(MUST_BUILD_CLIENT),yes)
  PROGS += pstream_client
endif

INCLUDES = -I. $(shell $(XMLRPC_C_CONFIG) c++2 client abyss-server --cflags)

LDADD_SERVER_ABYSS = \
  $(shell $(XMLRPC_C_CONFIG) c++2 abyss-server --ldadd)

LDADD_CLIENT = \
  $(shell $(XMLRPC_C_CONFIG) c++2 client --ldadd)

LDADD_BASE = \
  $(shell $(XMLRPC_C_CONFIG) c++2 --ldadd)

all: $(PROGS)

$(SERVERPROGS_ABYSS):%:%.o
	$(CXXLD) -o $@ $(LDFLAGS) $^ $(LDADD_SERVER_ABYSS)

$(CLIENTPROGS):%:%.o
	$(CXXLD) -o $@ $(LDFLAGS) $^ $(LDADD_CLIENT)


LDADD_PSTREAM_CLIENT = \
  $(shell $(XMLRPC_C_CONFIG) c++2 client --ldadd)

pstream_client:%:%.o
	$(CXXLD) -o $@ $(LDFLAGS) $^ $(LDADD_PSTREAM_CLIENT)

LDADD_PSTREAM_SERVER = \
  $(shell $(XMLRPC_C_CONFIG) c++2 pstream-server --ldadd)

pstream_inetd_server:%:%.o
	$(CXXLD) -o $@ $(LDFLAGS) $^ $(LDADD_PSTREAM_SERVER)

OBJECTS = $(PROGS:%=%.o)

$(OBJECTS):%.o:%.cpp
	$(CXX) -c $(INCLUDES) $(CXXFLAGS) $<

# See example/Makefile for an explanation of config.h and xmlrpc_amconfig.h

$(OBJECTS): config.h xmlrpc_amconfig.h

config.h:
	$(LN_S) $(BLDDIR)/xmlrpc_config.h $@
xmlrpc_amconfig.h:
	$(LN_S) $(BLDDIR)/$@ .

include $(SRCDIR)/common.mk

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

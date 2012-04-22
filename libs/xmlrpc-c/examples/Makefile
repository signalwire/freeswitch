# Since the programs in this directories are examples for the user, this
# make file should be as ordinary as possible.  It should not rely heavily
# on included make files or configuration parameters.  It should not use
# libtool.  Also, we don't try to build or rebuild the libraries on which
# these programs depend.


ifeq ($(SRCDIR),)
SRCDIR = $(CURDIR)/..
BLDDIR = $(SRCDIR)
endif
SUBDIR = examples

include $(BLDDIR)/config.mk

default: all

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)
LDFLAGS = $(LADD)

# If this were a real application, working from an installed copy of
# Xmlrpc-c, XMLRPC_C_CONFIG would just be 'xmlrpc-c-config'.  It would be
# found in the user's PATH.
XMLRPC_C_CONFIG = $(BLDDIR)/xmlrpc-c-config.test

CLIENTPROGS = \
  auth_client \
  synch_client \
  xmlrpc_sample_add_client \
  xmlrpc_asynch_client \

ifeq ($(MUST_BUILD_CURL_CLIENT),yes)
  CLIENTPROGS += interrupted_client
endif

SERVERPROGS_CGI = \
  xmlrpc_sample_add_server.cgi

SERVERPROGS_ABYSS = \
  interrupted_server \
  xmlrpc_inetd_server \
  xmlrpc_socket_server \
  xmlrpc_loop_server \
  xmlrpc_sample_add_server \
  xmlrpc_server_validatee \

# Build up PROGS:
PROGS = 

ifeq ($(ENABLE_ABYSS_SERVER),yes)
  PROGS += $(SERVERPROGS_ABYSS)
endif

PROGS += gen_sample_add_xml

ifeq ($(MUST_BUILD_CLIENT),yes)
  PROGS += $(CLIENTPROGS)
endif

ifeq ($(ENABLE_CGI_SERVER),yes)
  PROGS += $(SERVERPROGS_CGI) 
endif

INCLUDES = -I. $(shell $(XMLRPC_C_CONFIG) client abyss-server --cflags)

LDADD_CLIENT = \
  $(shell $(XMLRPC_C_CONFIG) client --ldadd)

LDADD_SERVER_ABYSS = \
  $(shell $(XMLRPC_C_CONFIG) abyss-server --ldadd)

LDADD_SERVER_CGI = \
  $(shell $(XMLRPC_C_CONFIG) cgi-server --ldadd)

LDADD_BASE = \
  $(shell $(XMLRPC_C_CONFIG) --ldadd)

all: $(PROGS)

ifeq ($(ENABLE_CPLUSPLUS),yes)
  all: cpp/all
endif

.PHONY: cpp/all
cpp/all: $(BLDDIR)/examples/cpp
	$(MAKE) -C cpp -f $(SRCDIR)/examples/cpp/Makefile all

# When building in separate tree, directory won't exist yet
$(BLDDIR)/examples/cpp:
	mkdir $@

$(CLIENTPROGS):%:%.o
	$(CCLD) -o $@ $(LDFLAGS) $^ $(LDADD_CLIENT)

$(SERVERPROGS_CGI):%.cgi:%_cgi.o
	$(CCLD) -o $@ $(LDFLAGS) $^ $(LDADD_SERVER_CGI)

$(SERVERPROGS_ABYSS):%:%.o
	$(CCLD) -o $@ $(LDFLAGS) $^ $(LDADD_SERVER_ABYSS)

gen_sample_add_xml:%:%.o
	$(CCLD) -o $@ $(LDFLAGS) $^ $(LDADD_BASE)

OBJECTS = $(patsubst %,%.o,$(patsubst %.cgi,%_cgi,$(PROGS)))

$(OBJECTS):%.o:%.c
	$(CC) -c $(INCLUDES) $(CFLAGS) $<

# config.h and xmlrpc_amconfig.h just describe the build environment.
# We use them so that the example programs will build in users'
# various environments.  If you're copying these examples, you can
# just remove these headers from the programs and hardcode whatever is
# right for your build environment.

$(OBJECTS): config.h xmlrpc_amconfig.h

config.h:
	$(LN_S) $(BLDDIR)/xmlrpc_config.h $@
xmlrpc_amconfig.h:
	$(LN_S) $(BLDDIR)/$@ .

include $(SRCDIR)/common.mk

.PHONY: clean
clean: clean-common
	rm -f $(PROGS) config.h xmlrpc_amconfig.h
	$(MAKE) -C cpp clean

.PHONY: distclean
distclean: clean

BINDIR=$(DESTDIR)$(bindir)

FILENAME_GENERATOR = "echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'"

INSTCMD =  "$(LIBTOOL) --mode=install $(INSTALL_PROGRAM) $$p \
  $(BINDIR)/`$(FILENAME_GENERATOR)`"

.PHONY: install
install: $(PROGS)
	@$(NORMAL_INSTALL)
	$(MKINSTALLDIRS) $(BINDIR)
	@list='$(bin_PROGRAMS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo "$(INSTCMD)"; $(INSTCMD); \
	  else :; \
          fi; \
	done

.PHONY: check
check:

.PHONY: dep depend
dep depend:
# We don't do dependencies in this directory, because it's supposed to be
# an example of what a program outside this package would do, so we can't
# go weaving it into the rest of the package.  Ergo, a developer must
# carefully clean and remake examples as he updates other parts of the tree.

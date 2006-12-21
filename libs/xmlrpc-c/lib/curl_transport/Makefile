ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
endif

include $(SRCDIR)/Makefile.config

CURL_INCLUDES := $(shell curl-config --cflags)
# We expect that curl-config --cflags just gives us -I options, because
# we need just the -I options for 'make dep'.  Plus, it's scary to think
# of what any other compiler flag would do to our compile.

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)
LDFLAGS = -lpthread $(LADD)

INCLUDES = -I$(SRCDIR) -I$(SRCDIR)/include -I$(SRCDIR)/lib/util/include \
  $(CURL_INCLUDES)

default: all

.PHONY: all
all: xmlrpc_curl_transport.lo

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

include $(SRCDIR)/Makefile.common

include Makefile.depend

xmlrpc_curl_transport.lo:%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) $(CFLAGS) $<

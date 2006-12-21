ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
endif

include $(SRCDIR)/Makefile.config

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)
LDFLAGS = -lpthread $(LADD)

INCLUDES = -I$(SRCDIR) -I$(SRCDIR)/src

default: all

.PHONY: all
all: xmlrpc_wininet_transport.lo

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

xmlrpc_wininet_transport.lo:%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) $(CFLAGS) $<

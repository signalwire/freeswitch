ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
endif

include $(SRCDIR)/Makefile.config

LIBWWW_INCLUDES := $(shell libwww-config --cflags)

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)
LDFLAGS = -lpthread $(LADD)

INCLUDES = -I$(SRCDIR) -I$(SRCDIR)/include -I$(SRCDIR)/lib/util/include \
  $(LIBWWW_INCLUDES)

default: all

.PHONY: all
all: xmlrpc_libwww_transport.lo

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

xmlrpc_libwww_transport.lo:%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) $(CFLAGS) $<



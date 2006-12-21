ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
BUILDDIR = $(SRCDIR)
endif

all: xmlrpc

include $(BUILDDIR)/Makefile.config

PROGRAMS_TO_INSTALL = xmlrpc

include ../Makefile.common

INCLUDES = -I$(SRCDIR) -I$(SRCDIR)/include -I$(SRCDIR)/lib/util/include

CFLAGS = $(CFLAGS_COMMON) $(INCLUDES) $(CFLAGS_PERSONAL) $(CADD)

LDFLAGS = $(CLIENT_LDFLAGS) $(LADD)

UTIL_OBJS = cmdline_parser.o getoptx.o
UTILS = $(UTIL_OBJS:%=$(UTIL_DIR)/%)

xmlrpc:%:%.o $(LIBXMLRPC_CLIENT) $(LIBXMLRPC) $(LIBXML) $(UTILS)
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LDFLAGS) $^

%.o:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(CFLAGS) $<

*.c: config.h

config.h:
	$(LN_S) $(BUILDDIR)/xmlrpc_config.h $@

include Makefile.depend

.PHONY: dep
dep: dep-common

.PHONY: clean
clean: clean-common
	rm -f xmlrpc config.h

.PHONY: distclean
distclean: clean distclean-common

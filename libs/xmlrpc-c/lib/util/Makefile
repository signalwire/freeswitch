ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
BUILDDIR = $(SRCDIR)
endif

default: all

include $(BUILDDIR)/Makefile.config

include $(SRCDIR)/Makefile.common

.PHONY: all
all: casprintf.lo cmdline_parser.lo getoptx.lo

INCLUDES = -Iinclude -I$(BUILDDIR)

CFLAGS = $(CFLAGS_COMMON) $(INCLUDES) $(CFLAGS_PERSONAL) $(CADD)

%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(CFLAGS) $<

include Makefile.depend

.PHONY: clean distclean
clean: clean-common

distclean: clean distclean-common

.PHONY: dep
dep: dep-common

install:
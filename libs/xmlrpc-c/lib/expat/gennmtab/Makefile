ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../../..
endif

include $(SRCDIR)/Makefile.config

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

LDFLAGS = $(LADD)

INCLUDES = -I$(SRCDIR) -I$(SRCDIR)/lib/util/include

default: all

include $(SRCDIR)/Makefile.common

.PHONY: all
all: gennmtab

.PHONY: clean
clean: clean-common
	rm -f gennmtab

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

gennmtab.o:%.o:%.c
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDES)

gennmtab:%:%.o
	$(LINK) -o $@ $(LDFLAGS) $(CFLAGS) $^

include Makefile.depend

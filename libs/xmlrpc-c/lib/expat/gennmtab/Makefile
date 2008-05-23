ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  EXPATDIR := $(call updir,$(CURDIR))
  LIBDIR := $(call updir,$(EXPATDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/expat/gennmtab

include $(BLDDIR)/config.mk

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

LDFLAGS = $(LADD)

INCLUDES = -I$(BLDDIR) -I$(SRCDIR)/lib/util/include

default: all

include $(SRCDIR)/common.mk

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
	$(BUILDTOOL_CC) -c $< -o $@ $(CFLAGS) $(INCLUDES)

gennmtab:%:%.o
	$(BUILDTOOL_CCLD) -o $@ $(LDFLAGS) $^

include Makefile.depend

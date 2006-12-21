ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
endif
SUBDIR = lib/abyss

include $(SRCDIR)/Makefile.config

SUBDIRS = src

default: all

.PHONY: all
all: $(SUBDIRS:%=%/all)

.PHONY: clean
clean: $(SUBDIRS:%=%/clean) clean-common

.PHONY: distclean
distclean: $(SUBDIRS:%=%/distclean) distclean-common

.PHONY: tags
tags: $(SUBDIRS:%=%/tags) TAGS

DISTFILES = 

.PHONY: distdir
distdir: distdir-common

.PHONY: install
install: $(SUBDIRS:%=%/install)

.PHONY: dep
dep: $(SUBDIRS:%=%/dep)

include $(SRCDIR)/Makefile.common




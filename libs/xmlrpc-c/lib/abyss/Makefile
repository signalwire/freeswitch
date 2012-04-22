ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  LIBDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/abyss

include $(BLDDIR)/config.mk

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

include $(SRCDIR)/common.mk




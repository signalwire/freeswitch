ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  SRCDIR := $(call updir,$(CURDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib

include $(BLDDIR)/config.mk

# Build up SUBDIRS:
SUBDIRS = 
SUBDIRS += util libutil
ifeq ($(ENABLE_ABYSS_SERVER),yes)
  SUBDIRS += abyss
endif
ifeq ($(MUST_BUILD_WININET_CLIENT),yes)
  SUBDIRS += wininet_transport
endif
ifeq ($(MUST_BUILD_CURL_CLIENT),yes)
  SUBDIRS += curl_transport
endif
ifeq ($(MUST_BUILD_LIBWWW_CLIENT),yes)
  SUBDIRS += libwww_transport
endif
ifneq ($(ENABLE_LIBXML2_BACKEND),yes)
  SUBDIRS += expat
endif

default: all

.PHONY: all clean distclean tags distdir install check dep

all: $(SUBDIRS:%=%/all)

# Extra dependencies to make parallel make work in spite of all the submakes
# (See top level make file for details)
abyss/all curl_transport/all: $(BLDDIR)/version.h

clean: $(SUBDIRS:%=%/clean) clean-common

distclean: $(SUBDIRS:%=%/distclean) distclean-common

tags: $(SUBDIRS:%=%/tags) TAGS

DISTFILES = 

distdir: distdir-common

install: $(SUBDIRS:%=%/install)

check:

dep: $(SUBDIRS:%=%/dep)

include $(SRCDIR)/common.mk




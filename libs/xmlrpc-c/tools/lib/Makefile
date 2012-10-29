ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  TOOLSDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(TOOLSDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := tools/lib

default: all

include $(BLDDIR)/config.mk

OMIT_LIB_RULE = Y

include $(SRCDIR)/tools/common.mk

INCLUDES = \
  -Isrcdir/$(SUBDIR)/include \
  -Iblddir \
  -Isrcdir/include \
  -Isrcdir/lib/util/include \

LIBOBJS = \
  dumpvalue.o \

.PHONY: all
all: $(LIBOBJS)

%.o:%.c
	$(CC) -c $(CFLAGS_ALL) $<

# This common.mk dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/tools/common.mk: srcdir blddir

include depend.mk

.PHONY: dep
dep: dep-common

.PHONY: clean
clean: clean-common

.PHONY: distclean
distclean: clean distclean-common

ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  ABYSSDIR := $(call updir,$(CURDIR))
  LIBDIR := $(call updir,$(ABYSSDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/abyss/src

include $(BLDDIR)/config.mk

default: all

TARGET_LIBRARY_NAMES := libxmlrpc_abyss

STATIC_LIBRARIES_TO_INSTALL = libxmlrpc_abyss.a

SHARED_LIBS_TO_BUILD := libxmlrpc_abyss
SHARED_LIBS_TO_INSTALL := libxmlrpc_abyss

ifeq ($(ENABLE_ABYSS_THREADS),yes)
  THREAD_MODULE = thread_pthread
else
  THREAD_MODULE = thread_fork
endif

TARGET_MODS = \
  channel \
  chanswitch \
  conf \
  conn \
  data \
  date \
  file \
  handler \
  http \
  init \
  response \
  server \
  session \
  socket \
  socket_unix \
  token \
  $(THREAD_MODULE) \
  trace \

OMIT_ABYSS_LIB_RULE = Y
MAJ=3
  # Major number of shared libraries in this directory

include $(SRCDIR)/common.mk

CFLAGS = $(CFLAGS_COMMON) -D_UNIX $(CFLAGS_PERSONAL) $(CADD)

INCLUDES = -Iblddir -Isrcdir -Isrcdir/include -Isrcdir/lib/util/include

ABYSS_SHLIB = $(call shlibfn,libxmlrpc_abyss)
#ABYSS_SHLIB is e.g. libxmlrpc_abyss.so.3.1
ABYSS_SHLIBLE = $(call shliblefn,libxmlrpc_abyss)
#ABYSS_SHLIBLE is e.g. libxmlrpc_abyss.so

# LIBDEP is the shared libraries on which libxmlrpc_abyss depends.
# The runtime loader should load these libraries when it loads libxmlrpc_abyss.

LIBDEP = $(LIBXMLRPC_UTIL)

ifneq ($(SHARED_LIB_TYPE),NONE)
  TARGET_SHARED_LIBS := $(ABYSS_SHLIB) $(ABYSS_SHLIBLE)
  endif

# This 'common.mk' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir blddir

.PHONY: all
all: libxmlrpc_abyss.a $(TARGET_SHARED_LIBS) $(TARGET_SHARED_LE_LIBS)


# Rule for this is in common.mk, courtesy of TARGET_SHARED_LIBRARIES:
$(ABYSS_SHLIB): $(TARGET_MODS:%=%.osh) $(LIBDEP)
$(ABYSS_SHLIB): LIBOBJECTS = $(TARGET_MODS:%=%.osh) $(LIBDEP)

# Rule for this is in common.mk, courtesy of TARGET_STATIC_LIBRARIES:

libxmlrpc_abyss.a: $(TARGET_MODS:%=%.o)
libxmlrpc_abyss.a: LIBOBJECTS = $(TARGET_MODS:%=%.o)

#-----------------------------------------------------------------------------
#  RULES TO COMPILE OBJECT MODULES FOR LIBRARIES
#-----------------------------------------------------------------------------

# Rules to compile object modules from which to build the static and shared
# Abyss library are in common.mk, courtesy of TARGET_MODS.


# Need this dependency for those who don't use Makefile.depend.
# Without it, version.h doesn't get created.
response.o response.osh handler.o handler.osh: version.h

.PHONY: clean
clean: clean-common

.PHONY: distclean
distclean: clean distclean-common

.PHONY: tags
tags: TAGS

.PHONY: distdir
distdir:

.PHONY: install
install: install-common

.PHONY: dep
dep: dep-common

include Makefile.depend

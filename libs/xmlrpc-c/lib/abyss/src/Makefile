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

CFLAGS_LOCAL = -D_UNIX

INCLUDES = -Iblddir -Isrcdir -Isrcdir/include -Isrcdir/lib/util/include

ABYSS_SHLIB = $(call shlibfn,libxmlrpc_abyss)
#ABYSS_SHLIB is e.g. libxmlrpc_abyss.so.3.1
ABYSS_SHLIBLE = $(call shliblefn,libxmlrpc_abyss)
#ABYSS_SHLIBLE is e.g. libxmlrpc_abyss.so

# This 'common.mk' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir blddir

.PHONY: all
all: libxmlrpc_abyss.a $(TARGET_SHARED_LIBRARIES) $(TARGET_SHARED_LE_LIBS)


# Rule for this is in common.mk, courtesy of TARGET_LIBRARY_NAMES
$(ABYSS_SHLIB): $(TARGET_MODS:%=%.osh) \
	$(LIBXMLRPC_UTIL)
$(ABYSS_SHLIB): LIBOBJECTS = $(TARGET_MODS:%=%.osh)
$(ABYSS_SHLIB): LIBDEP = -L$(LIBXMLRPC_UTIL_DIR) -lxmlrpc_util
ifeq ($(ENABLE_ABYSS_THREADS),yes)
  $(ABYSS_SHLIB):  LIBDEP += -lpthread
endif

# Rule for this is in common.mk, courtesy of TARGET_STATIC_LIBRARIES:

libxmlrpc_abyss.a: $(TARGET_MODS:%=%.o)
libxmlrpc_abyss.a: LIBOBJECTS = $(TARGET_MODS:%=%.o)

#-----------------------------------------------------------------------------
#  RULES TO COMPILE OBJECT MODULES FOR LIBRARIES
#-----------------------------------------------------------------------------

# Rules to compile object modules from which to build the static and shared
# Abyss library are in common.mk, courtesy of TARGET_MODS.


# Need this dependency for those who don't use depend.mk.
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

include depend.mk

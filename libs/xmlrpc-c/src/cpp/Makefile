ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  srcDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(srcDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := src/cpp

include $(BLDDIR)/config.mk

default: all

# libxmlrpc_cpp is the legacy C++ wrapper library.  The others are the
# more elaborate replacements.

TARGET_LIB_NAMES_PP := \
  libxmlrpc_cpp \
  libxmlrpc++ \
  libxmlrpc_server++ \
  libxmlrpc_server_abyss++ \
  libxmlrpc_server_pstream++ \
  libxmlrpc_packetsocket \

ifeq ($(MUST_BUILD_CLIENT),yes)
  TARGET_LIB_NAMES_PP += libxmlrpc_client++
endif

STATIC_LIBRARIES_TO_INSTALL = $(TARGET_STATIC_LIBRARIES)

SHARED_LIBS_TO_BUILD := $(TARGET_LIB_NAMES_PP)
SHARED_LIBS_TO_INSTALL := $(TARGET_LIB_NAMES_PP)

# INCLUDES and DEP_SOURCES are used by dep-common target
INCLUDES = $(BASIC_INCLUDES) $(CLIENT_INCLUDES) $(LIBXML_INCLUDES) \
  $(SERVER_INCLUDES) $(SERVER_ABYSS_INCLUDES) $(TRANSPORT_INCLUDES)
DEP_SOURCES = *.cpp

WININET_TRANSPORT_DIR = srcdir/lib/wininet_transport
CURL_TRANSPORT_DIR    = srcdir/lib/curl_transport
LIBWWW_TRANSPORT_DIR  = srcdir/lib/libwww_transport

ifeq ($(ENABLE_LIBXML2_BACKEND),yes)
  LIBXML_INCLUDES = $(LIBXML2_CFLAGS)
else
  LIBXML_INCLUDES = -Isrcdir/lib/expat/xmlparse
endif

LIBXMLRPCPP_MODS = \
  base64 env_wrap fault girerr girmem outcome param_list value xml
LIBXMLRPC_SERVERPP_MODS = registry
LIBXMLRPC_SERVER_ABYSSPP_MODS = server_abyss
LIBXMLRPC_SERVER_PSTREAMPP_MODS = server_pstream
LIBXMLRPC_CLIENTPP_MODS = client client_simple curl libwww wininet pstream
LIBXMLRPC_PACKETSOCKET_MODS = packetsocket

TARGET_MODS_PP = \
  XmlRpcCpp \
  $(LIBXMLRPCPP_MODS) \
  $(LIBXMLRPC_SERVERPP_MODS) \
  $(LIBXMLRPC_SERVER_ABYSSPP_MODS) \
  $(LIBXMLRPC_SERVER_PSTREAMPP_MODS) \
  $(LIBXMLRPC_CLIENTPP_MODS) \
  $(LIBXMLRPC_PACKETSOCKET_MODS) \

OMIT_CPP_LIB_RULES = Y
MAJ=4
  # Major number of shared libraries in this directory

include $(SRCDIR)/common.mk


# This 'common.mk' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir blddir

TRANSPORT_INCLUDES = \
  -I$(WININET_TRANSPORT_DIR) \
  -I$(CURL_TRANSPORT_DIR) \
  -I$(LIBWWW_TRANSPORT_DIR) \

BASIC_INCLUDES = \
  -Iblddir/include \
  -Isrcdir/include \
  -Iblddir \
  -Isrcdir/lib/util/include


# TARGET_STATIC_LIBARIES, etc. are set by common.mk, based on
# TARGET_LIB_NAMES_PP.

all: \
  $(TARGET_STATIC_LIBRARIES) \
  $(TARGET_SHARED_LIBS_PP) \
  $(TARGET_SHARED_LE_LIBS)


#-----------------------------------------------------------------------------
#  RULES TO LINK LIBRARIES
#-----------------------------------------------------------------------------

# Rules for these are in common.mk, courtesy of TARGET_LIB_NAMES_PP:

# shlibfn generates e.g. libxmlrpc.so.3.1
# shliblefn generates e.g. libxmlrpc.so

LIBXMLRPC_CPP = $(call shlibfn, libxmlrpc_cpp)

$(LIBXMLRPC_CPP): XmlRpcCpp.osh
$(LIBXMLRPC_CPP): LIBOBJECTS = XmlRpcCpp.osh

LIBXMLRPCPP = $(call shlibfn, libxmlrpc++)

$(LIBXMLRPCPP): $(LIBXMLRPCPP_MODS:%=%.osh) \
  $(LIBXMLRPC)
$(LIBXMLRPCPP): LIBOBJECTS = $(LIBXMLRPCPP_MODS:%=%.osh)
$(LIBXMLRPCPP): LIBDEP = -Lblddir/src -lxmlrpc

LIBXMLRPC_SERVERPP = $(call shlibfn, libxmlrpc_server++)

$(LIBXMLRPC_SERVERPP): $(LIBXMLRPC_SERVERPP_MODS:%=%.osh) \
  $(LIBXMLRPC_SERVER) \
  $(LIBXMLRPC)
$(LIBXMLRPC_SERVERPP): LIBOBJECTS = $(LIBXMLRPC_SERVERPP_MODS:%=%.osh)
$(LIBXMLRPC_SERVERPP): LIBDEP = -Lblddir/src -lxmlrpc_server -lxmlrpc

LIBXMLRPC_SERVER_ABYSSPP = $(call shlibfn, libxmlrpc_server_abyss++)

$(LIBXMLRPC_SERVER_ABYSSPP): $(LIBXMLRPC_SERVER_ABYSSPP_MODS:%=%.osh) \
  $(LIBXMLRPC_ABYSS) \
  $(LIBXMLRPC)
$(LIBXMLRPC_SERVER_ABYSSPP): LIBOBJECTS = $(LIBXMLRPC_SERVER_ABYSSPP_MODS:%=%.osh)
$(LIBXMLRPC_SERVER_ABYSSPP): LIBDEP = \
  -L$(LIBXMLRPC_ABYSS_DIR) -lxmlrpc_abyss -Lblddir/src -lxmlrpc

LIBXMLRPC_SERVER_PSTREAMPP = $(call shlibfn, libxmlrpc_server_pstream++)

$(LIBXMLRPC_SERVER_PSTREAMPP): $(LIBXMLRPC_SERVER_PSTREAMPP_MODS:%=%.osh) \
  $(LIBXMLRPC_SERVER) \
  $(LIBXMLRPC_PACKETSOCKET)
$(LIBXMLRPC_SERVER_PSTREAMPP): LIBOBJECTS = $(LIBXMLRPC_SERVER_PSTREAMPP_MODS:%=%.osh)
$(LIBXMLRPC_SERVER_PSTREAMPP): LIBDEP = \
  -Lblddir/src -lxmlrpc_server -L. -lxmlrpc_packetsocket

LIBXMLRPC_CLIENTPP = $(call shlibfn, libxmlrpc_client++)

$(LIBXMLRPC_CLIENTPP): $(LIBXMLRPC_CLIENTPP_MODS:%=%.osh) \
  $(LIBXMLRPCPP) \
  $(LIBXMLRPC_CLIENT)
$(LIBXMLRPC_CLIENTPP): LIBOBJECTS = $(LIBXMLRPC_CLIENTPP_MODS:%=%.osh)
$(LIBXMLRPC_CLIENTPP): LIBDEP = \
  -L. -lxmlrpc++ -Lblddir/src -lxmlrpc_client

LIBXMLRPC_PACKETSOCKET = $(call shlibfn, libxmlrpc_packetsocket)

$(LIBXMLRPC_PACKETSOCKET): $(LIBXMLRPC_PACKETSOCKET_MODS:%=%.osh)
$(LIBXMLRPC_PACKETSOCKET): LIBOBJECTS = $(LIBXMLRPC_PACKETSOCKET_MODS:%=%.osh)

libxmlrpc_cpp.a: XmlRpcCpp.o
libxmlrpc_cpp.a: LIBOBJECTS = XmlRpcCpp.o

libxmlrpc++.a: $(LIBXMLRPCPP_MODS:%=%.o)
libxmlrpc++.a: LIBOBJECTS = $(LIBXMLRPCPP_MODS:%=%.o)

libxmlrpc_server++.a: $(LIBXMLRPC_SERVERPP_MODS:%=%.o)
libxmlrpc_server++.a: LIBOBJECTS = $(LIBXMLRPC_SERVERPP_MODS:%=%.o)

libxmlrpc_server_abyss++.a: $(LIBXMLRPC_SERVER_ABYSSPP_MODS:%=%.o)
libxmlrpc_server_abyss++.a: LIBOBJECTS=$(LIBXMLRPC_SERVER_ABYSSPP_MODS:%=%.o)

libxmlrpc_server_pstream++.a: $(LIBXMLRPC_SERVER_PSTREAMPP_MODS:%=%.o)
libxmlrpc_server_pstream++.a: LIBOBJECTS=$(LIBXMLRPC_SERVER_PSTREAMPP_MODS:%=%.o)

libxmlrpc_client++.a: $(LIBXMLRPC_CLIENTPP_MODS:%=%.o)
libxmlrpc_client++.a: LIBOBJECTS = $(LIBXMLRPC_CLIENTPP_MODS:%=%.o)

libxmlrpc_packetsocket.a: $(LIBXMLRPC_PACKETSOCKET_MODS:%=%.o)
libxmlrpc_packetsocket.a: LIBOBJECTS = $(LIBXMLRPC_PACKETSOCKET_MODS:%=%.o)

#-----------------------------------------------------------------------------
#  RULES TO COMPILE OBJECT MODULES FOR LIBRARIES
#-----------------------------------------------------------------------------

# Rules for these are in common.mk, courtesy of TARGET_MODS_PP:

XmlRpcCpp.o% XmlrpcCpp.osh: \
  INCLUDES = $(BASIC_INCLUDES)

$(LIBXMLRPCPP_MODS:%=%.o) \
$(LIBXMLRPCPP_MODS:%=%.osh): \
  INCLUDES = $(BASIC_INCLUDES)

SERVER_INCLUDES = $(BASIC_INCLUDES) $(LIBXML_INCLUDES)

$(LIBXMLRPC_SERVERPP_MODS:%=%.o) \
$(LIBXMLRPC_SERVERPP_MODS:%=%.osh): \
  INCLUDES = $(SERVER_INCLUDES)

$(LIBXMLRPC_SERVER_ABYSSPP_MODS:%=%.o) \
$(LIBXMLRPC_SERVER_ABYSSPP_MODS:%=%.o): \
  INCLUDES = $(SERVER_INCLUDES)

$(LIBXMLRPC_SERVER_PSTREAMPP_MODS:%=%.o) \
$(LIBXMLRPC_SERVER_PSTREAMPP_MODS:%=%.o): \
  INCLUDES = $(SERVER_INCLUDES)

$(LIBXMLRPC_CLIENTPP_MODS:%=%.o) \
$(LIBXMLRPC_CLIENTPP_MODS:%=%.osh): \
  INCLUDES = $(BASIC_INCLUDES) $(LIBXML_INCLUDES) $(TRANSPORT_INCLUDES)

$(LIBXMLRPC_PACKETSOCKET_MODS:%=%.o) \
$(LIBXMLRPC_PACKETSOCKET_MODS:%=%.osh): \
  INCLUDES = $(BASIC_INCLUDES)

CXXFLAGS = $(CXXFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

TRANSPORT_CONFIG_USERS = client curl libwww wininet

$(TRANSPORT_CONFIG_USERS:%=%.o) $(TRANSPORT_CONFIG_USERS:%=%.osh): \
  $(BLDDIR)/transport_config.h

#-----------------------------------------------------------------------------
#  MISCELLANEOUS RULES
#-----------------------------------------------------------------------------

check:
	$(MAKE) -C test runtests

.PHONY: install
install: install-common

.PHONY: clean clean-local distclean distclean-local
clean: clean-common clean-local
clean-local:
	$(MAKE) -C test clean

distclean: clean distclean-local distclean-common

distclean-local:
	$(MAKE) -C test distclean

.PHONY: dep
dep: dep-common $(BLDDIR)/transport_config.h

include Makefile.depend

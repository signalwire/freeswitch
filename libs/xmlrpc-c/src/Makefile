ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  SRCDIR := $(call updir,$(CURDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := src

include $(BLDDIR)/config.mk

default: all

SUBDIRS =

ifeq ($(ENABLE_CPLUSPLUS),yes)
  SUBDIRS += cpp
endif

TARGET_LIBRARY_NAMES := libxmlrpc libxmlrpc_server

# We cannot build libxmlrpc_server_abyss if we can't also build Abyss,
# because we need libxmlrpc_abyss in order to build the proper runtime
# dependencies into libxmlrpc_server_abyss.

ifeq ($(ENABLE_ABYSS_SERVER),yes)
  TARGET_LIBRARY_NAMES += libxmlrpc_server_abyss
endif

ifeq ($(MUST_BUILD_CLIENT),yes)
  TARGET_LIBRARY_NAMES += libxmlrpc_client
endif

ifeq ($(ENABLE_CGI_SERVER),yes)
  TARGET_LIBRARY_NAMES += libxmlrpc_server_cgi
endif

STATIC_LIBRARIES_TO_INSTALL = $(TARGET_STATIC_LIBRARIES)

SHARED_LIBS_TO_BUILD := $(TARGET_LIBRARY_NAMES)
SHARED_LIBS_TO_INSTALL := $(TARGET_LIBRARY_NAMES)

# TRANSPORT_MODS is the list of modules that have to go into the client
# library to provide the client XML transport functions.

# TRANSPORT_LIBDEP is linker -l options to declare what libraries contain
# things to which the transport objects refer.  (like LIBxxx_LIBDEP --
# see below)

TRANSPORT_MODS =
TRANSPORT_LIBDEP =
ifeq ($(MUST_BUILD_WININET_CLIENT),yes)
  TRANSPORT_MODS += $(BLDDIR)/lib/wininet_transport/xmlrpc_wininet_transport
  TRANSPORT_LIBDEP += $(shell wininet-config --libs)
endif
ifeq ($(MUST_BUILD_CURL_CLIENT),yes)
  TRANSPORT_MODS += $(BLDDIR)/lib/curl_transport/xmlrpc_curl_transport
  TRANSPORT_LIBDEP += $(shell curl-config --libs)
endif
ifeq ($(MUST_BUILD_LIBWWW_CLIENT),yes)
  TRANSPORT_MODS += $(BLDDIR)/lib/libwww_transport/xmlrpc_libwww_transport
  TRANSPORT_LIBDEP += $(shell libwww-config --libs)
endif

ifeq ($(ENABLE_LIBXML2_BACKEND),yes)
  XMLRPC_XML_PARSER = xmlrpc_libxml2
  XML_PARSER_LIBDEP = $(shell xml2-config --libs)
  XML_PARSER_LIBDEP_DEP =
else
  XMLRPC_XML_PARSER = xmlrpc_expat
  XML_PARSER_LIBDEP = -L$(BLDDIR)/lib/expat/xmlparse -lxmlrpc_xmlparse
  XML_PARSER_LIBDEP_DEP = $(LIBXMLRPC_XMLPARSE)
endif

# LIBxxx_OBJS is the list of object files that make up library libxxx.

LIBXMLRPC_MODS = \
        double \
	parse_value \
        resource \
	trace \
	version \
	xmlrpc_data \
	xmlrpc_datetime \
	xmlrpc_string \
	xmlrpc_array \
	xmlrpc_struct \
	xmlrpc_build \
	xmlrpc_decompose \
	$(XMLRPC_XML_PARSER) \
	xmlrpc_parse \
	xmlrpc_serialize \
	xmlrpc_base64 \
	xmlrpc_authcookie \

LIBXMLRPC_CLIENT_MODS = xmlrpc_client xmlrpc_client_global xmlrpc_server_info

LIBXMLRPC_SERVER_MODS = registry method system_method

LIBXMLRPC_SERVER_ABYSS_MODS = xmlrpc_server_abyss

LIBXMLRPC_SERVER_CGI_MODS = xmlrpc_server_cgi

TARGET_MODS = \
  $(LIBXMLRPC_MODS) \
  $(LIBXMLRPC_SERVER_MODS) \
  $(LIBXMLRPC_SERVER_ABYSS_MODS) \
  $(LIBXMLRPC_SERVER_CGI_MODS) \
  $(LIBXMLRPC_CLIENT_MODS) \

OMIT_XMLRPC_LIB_RULE=Y
MAJ=3
  # Major number of shared libraries in this directory

include $(SRCDIR)/common.mk

# This 'common.mk' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir blddir

# TARGET_STATIC_LIBRARIES, etc. are set by common.mk, based on
# TARGET_LIBRARY_NAMES.


all: \
  $(TARGET_STATIC_LIBRARIES) \
  $(TARGET_SHARED_LIBRARIES) \
  $(TARGET_SHARED_LE_LIBS) \
  $(SUBDIRS:%=%/all) \

# Extra dependencies to make parallel make work in spite of all the submakes
# (See top level make file for details)
ifeq ($MUST_BUILD_CLIENT),yes)
cpp/all: $(BLDDIR)/transport_config.h
endif

#-----------------------------------------------------------------------------
#  RULES TO LINK LIBRARIES
#-----------------------------------------------------------------------------

# Rules for these are in common.mk, courtesy of TARGET_LIBRARY_NAMES:

# shlibfn generates e.g. libxmlrpc.so.3.1
# shliblefn generates e.g. libxmlrpc.so

LIBXMLRPC = $(call shlibfn, libxmlrpc)

$(LIBXMLRPC): $(LIBXMLRPC_MODS:%=%.osh) \
  $(LIBXMLRPC_UTIL) \
  $(XML_PARSER_LIBDEP_DEP)
$(LIBXMLRPC): LIBOBJECTS = $(LIBXMLRPC_MODS:%=%.osh)
$(LIBXMLRPC): LIBDEP = \
  -L$(LIBXMLRPC_UTIL_DIR) -lxmlrpc_util \
  $(XML_PARSER_LIBDEP)

LIBXMLRPC_SERVER = $(call shlibfn, libxmlrpc_server)

$(LIBXMLRPC_SERVER): \
  $(LIBXMLRPC_SERVER_MODS:%=%.osh) \
  $(LIBXMLRPC_UTIL) \
  $(call shliblefn, libxmlrpc)
$(LIBXMLRPC_SERVER): LIBOBJECTS = $(LIBXMLRPC_SERVER_MODS:%=%.osh)
$(LIBXMLRPC_SERVER): LIBDEP = \
  -L$(LIBXMLRPC_UTIL_DIR) -lxmlrpc_util -L. -lxmlrpc

LIBXMLRPC_SERVER_ABYSS = $(call shlibfn, libxmlrpc_server_abyss)

$(LIBXMLRPC_SERVER_ABYSS): \
  $(LIBXMLRPC_SERVER_ABYSS_MODS:%=%.osh) \
  $(LIBXMLRPC_UTIL) \
  $(LIBXMLRPC_ABYSS) \
  $(call shliblefn, libxmlrpc_server) \
  $(call shliblefn, libxmlrpc)
$(LIBXMLRPC_SERVER_ABYSS): LIBOBJECTS = $(LIBXMLRPC_SERVER_ABYSS_MODS:%=%.osh)
$(LIBXMLRPC_SERVER_ABYSS): LIBDEP = \
  -L$(LIBXMLRPC_UTIL_DIR) -lxmlrpc_util \
  -L. -lxmlrpc_server \
  -L$(LIBXMLRPC_ABYSS_DIR) -lxmlrpc_abyss \
  -lxmlrpc

LIBXMLRPC_SERVER_CGI = $(call shlibfn, libxmlrpc_server_cgi)

$(LIBXMLRPC_SERVER_CGI): \
  $(LIBXMLRPC_SERVER_CGI_MODS:%=%.osh) \
  $(LIBXMRPC_UTIL) \
  $(call shliblefn, libxmlrpc_server) \
  $(call shliblefn, libxmlrpc)
$(LIBXMLRPC_SERVER_CGI): LIBOBJECTS = $(LIBXMLRPC_SERVER_CGI_MODS:%=%.osh)
$(LIBXMLRPC_SERVER_CGI): LIBDEP = \
  -L$(LIBXMLRPC_UTIL_DIR) -lxmlrpc_util -L. -lxmlrpc_server -lxmlrpc

LIBXMLRPC_CLIENT = $(call shlibfn, libxmlrpc_client)

$(LIBXMLRPC_CLIENT): \
  $(LIBXMLRPC_CLIENT_MODS:%=%.osh) \
  $(TRANSPORT_MODS:%=%.osh) \
  $(LIBXMLRPC_UTIL) \
  $(call shliblefn, libxmlrpc)
$(LIBXMLRPC_CLIENT): LIBOBJECTS = \
  $(LIBXMLRPC_CLIENT_MODS:%=%.osh) \
  $(TRANSPORT_MODS:%=%.osh)
$(LIBXMLRPC_CLIENT): LIBDEP = \
  -L$(LIBXMLRPC_UTIL_DIR) -lxmlrpc_util \
  -L. -lxmlrpc \
  $(TRANSPORT_LIBDEP)


libxmlrpc.a: $(LIBXMLRPC_MODS:%=%.o)
libxmlrpc.a: LIBOBJECTS = $(LIBXMLRPC_MODS:%=%.o)

libxmlrpc_server.a: $(LIBXMLRPC_SERVER_MODS:%=%.o)
libxmlrpc_server.a: LIBOBJECTS = $(LIBXMLRPC_SERVER_MODS:%=%.o)

libxmlrpc_server_abyss.a: $(LIBXMLRPC_SERVER_ABYSS_MODS:%=%.o)
libxmlrpc_server_abyss.a: LIBOBJECTS=$(LIBXMLRPC_SERVER_ABYSS_MODS:%=%.o)

libxmlrpc_server_cgi.a: $(LIBXMLRPC_SERVER_CGI_MODS:%=%.o)
libxmlrpc_server_cgi.a: LIBOBJECTS=$(LIBXMLRPC_SERVER_CGI_MODS:%=%.o)

libxmlrpc_client.a: $(LIBXMLRPC_CLIENT_MODS:%=%.o) $(TRANSPORT_MODS:%=%.o)
libxmlrpc_client.a: LIBOBJECTS = \
  $(LIBXMLRPC_CLIENT_MODS:%=%.o) \
  $(TRANSPORT_MODS:%=%.o)


#-----------------------------------------------------------------------------
#  RULES TO COMPILE OBJECT MODULES FOR LIBRARIES
#-----------------------------------------------------------------------------

# Rules for these are in common.mk, courtesy of TARGET_MODS:

BASIC_INCLUDES = \
  -I$(BLDDIR) \
  -I$(BLDDIR)/include \
  -I$(SRCDIR)/include \
  -I$(SRCDIR)/lib/util/include \

TRANSPORT_INCLUDES = \
  -I$($SRCDIR)/lib/wininet_transport \
  -I$(SRCDIR)/lib/curl_transport \
  -I$(SRCDIR)/lib/libwww_transport \

ifeq ($(ENABLE_LIBXML2_BACKEND),yes)
  LIBXML_INCLUDES = $(shell xml2-config --cflags)
else
  LIBXML_INCLUDES = -I$(SRCDIR)/lib/expat/xmlparse
endif

$(LIBXMLRPC_MODS:%=%.o) \
$(LIBXMLRPC_MODS:%=%.osh): \
  INCLUDES = $(BASIC_INCLUDES) $(LIBXML_INCLUDES)

$(LIBXMLRPC_CLIENT_MODS:%=%.o) \
$(LIBXMLRPC_CLIENT_MODS:%=%.osh): \
  INCLUDES = $(BASIC_INCLUDES) $(TRANSPORT_INCLUDES)

$(LIBXMLRPC_SERVER_MODS:%=%.o) \
$(LIBXMLRPC_SERVER_MODS:%=%.osh): \
  INCLUDES = $(BASIC_INCLUDES)

$(LIBXMLRPC_SERVER_ABYSS_MODS:%=%.o) \
$(LIBXMLRPC_SERVER_ABYSS_MODS:%=%.osh): \
  INCLUDES = $(BASIC_INCLUDES) -I$(SRCDIR)/lib/abyss/src

$(LIBXMLRPC_SERVER_CGI_MODS:%=%.o) \
$(LIBXMLRPC_SERVER_CGI_MODS:%=%.osh): \
  INCLUDES = $(BASIC_INCLUDES)

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)


check:
	$(MAKE) -C test runtests
	$(MAKE) -C cpp  check

.PHONY: install
install: install-common $(SUBDIRS:%=%/install)

.PHONY: clean clean-local distclean
clean: clean-common clean-local $(SUBDIRS:%=%/clean)
clean-local:
	$(MAKE) -C test clean

distclean: clean-common clean-local distclean-common $(SUBDIRS:%=%/distclean)

.PHONY: dep
dep: $(SUBDIRS:%=%/dep) $(BLDDIR)/transport_config.h dep-common

dep-common:
  INCLUDES = $(BASIC_INCLUDES) $(TRANSPORT_INCLUDES) $(LIBXML_INCLUDES)

xmlrpc_client.o xmlrpc_client.osh: $(BLDDIR)/transport_config.h
xmlrpc_client.o xmlrpc_client.osh: $(BLDDIR)/version.h
registry.o registry.osh: $(BLDDIR)/version.h
version.o version.osh: $(BLDDIR)/version.h

include Makefile.depend

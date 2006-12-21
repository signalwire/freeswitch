ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/..
endif
SUBDIR = src
BUILDDIR = $(SRCDIR)
VPATH = .:$(SRCDIR)

include $(BUILDDIR)/Makefile.config

HEADERS_TO_INSTALL = \
  xmlrpc-c/oldxmlrpc.h \
  xmlrpc-c/base.h \
  xmlrpc-c/abyss.h \
  xmlrpc-c/server.h \
  xmlrpc-c/server_abyss.h \
  xmlrpc-c/server_w32httpsys.h \

ifeq ($(ENABLE_CPLUSPLUS),yes)
  HEADERS_TO_INSTALL += \
    xmlrpc-c/oldcppwrapper.hpp \
    xmlrpc-c/base.hpp \
    xmlrpc-c/timeout.hpp \
    xmlrpc-c/xml.hpp \
    xmlrpc-c/registry.hpp \
    xmlrpc-c/server_abyss.hpp\
    xmlrpc-c/girerr.hpp\
    xmlrpc-c/girmem.hpp\

endif



HEADERINST_PREFIX = /xmlrpc-c

ifeq ($(MUST_BUILD_CLIENT),yes)
  HEADERS_TO_INSTALL += \
    xmlrpc-c/client.h \
    xmlrpc-c/transport.h \

  ifeq ($(ENABLE_CPLUSPLUS),yes)
    HEADERS_TO_INSTALL += xmlrpc-c/client.hpp xmlrpc-c/client_simple.hpp
  endif
endif
ifeq ($(ENABLE_CGI_SERVER),yes)
  HEADERS_TO_INSTALL += xmlrpc-c/server_cgi.h
endif

default: all

all:

.PHONY: install
install: install-common

.PHONY: clean distclean dep
clean:
distclean:
dep:

include $(SRCDIR)/Makefile.common

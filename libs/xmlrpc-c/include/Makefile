ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  SRCDIR := $(call updir,$(CURDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := include

include $(BLDDIR)/config.mk

default: all

all: xmlrpc-c/config.h

xmlrpc-c/config.h: $(BLDDIR)/$(SUBDIR)/xmlrpc-c
	>$@
	@echo "Lots of echoes to '$@' suppressed here ..."
	@echo '#ifndef XMLRPC_C_CONFIG_H_INCLUDED'                        >>$@
	@echo '#define XMLRPC_C_CONFIG_H_INCLUDED'                        >>$@
	@echo ''                                                          >>$@
	@echo '/* This file, part of XML-RPC For C/C++, is meant to '     >>$@
	@echo '   define characteristics of this particular installation '>>$@ 
	@echo '   that the other <xmlrpc-c/...> header files need in '    >>$@
	@echo '   order to compile correctly when #included in Xmlrpc-c'  >>$@
	@echo '   user code.'						  >>$@
	@echo ''                                                          >>$@
	@echo '   Those header files #include this one.'                  >>$@
	@echo ''                                                          >>$@
	@echo '   This file was created by a make rule.'                  >>$@
	@echo '*/'                                                        >>$@
	@echo '#define XMLRPC_HAVE_WCHAR $(HAVE_WCHAR_H_DEFINE)'          >>$@
	@echo '#ifdef WIN32'                                              >>$@
	@echo '  /* SOCKET is a type defined by <winsock.h>.  Anyone who' >>$@
	@echo '     uses XMLRPC_SOCKET on a WIN32 system must #include'   >>$@
	@echo '     <winsock.h>'                                          >>$@
	@echo '  */'                                                      >>$@
	@echo '  #define XMLRPC_SOCKET SOCKET'                            >>$@
	@echo '  #define XMLRPC_HAVE_TIMEVAL 0'                           >>$@
	@echo '  #define XMLRPC_HAVE_TIMESPEC 0'                          >>$@
	@echo '#else'                                                     >>$@
	@echo '  #define XMLRPC_SOCKET int'                               >>$@
	@echo '  #define XMLRPC_HAVE_TIMEVAL 1'                           >>$@
	@echo '  #define XMLRPC_HAVE_TIMESPEC 1'                          >>$@
	@echo '#endif'                                                    >>$@
	@echo ''                                                          >>$@
	@echo '#if defined(_MSC_VER)'                                     >>$@
	@echo '  /* Newer MSVC has long long, but MSVC 6 does not */'     >>$@
	@echo '  #define XMLRPC_INT64 __int64'				  >>$@
	@echo '  #define XMLRPC_INT32 __int32'				  >>$@
	@echo '#else'							  >>$@
	@echo '  #define XMLRPC_INT64 long long'	                  >>$@
	@echo '  #define XMLRPC_INT32 int'		                  >>$@
	@echo '#endif'                                                    >>$@
	@echo '#endif'                                                    >>$@

$(BLDDIR)/$(SUBDIR)/xmlrpc-c:
	mkdir $@

COMPAT_LINK_CMDS = \
  $(LN_S) xmlrpc-c/oldxmlrpc.h         xmlrpc.h; \
  $(LN_S) xmlrpc-c/server.h            xmlrpc_server.h; \
  $(LN_S) xmlrpc-c/server_abyss.h      xmlrpc_server_abyss.h; \
  $(LN_S) xmlrpc-c/server_w32httpsys.h xmlrpc_server_w32httpsys.h; \

HEADERS_TO_INSTALL = \
  xmlrpc-c/config.h \
  xmlrpc-c/inttypes.h \
  xmlrpc-c/c_util.h \
  xmlrpc-c/util.h \
  xmlrpc-c/base.h \
  xmlrpc-c/abyss.h \
  xmlrpc-c/abyss_unixsock.h \
  xmlrpc-c/abyss_winsock.h \
  xmlrpc-c/server.h \
  xmlrpc-c/server_abyss.h \
  xmlrpc-c/server_w32httpsys.h \
  xmlrpc-c/oldxmlrpc.h \

ifeq ($(ENABLE_CPLUSPLUS),yes)
  HEADERS_TO_INSTALL += \
    xmlrpc-c/oldcppwrapper.hpp \
    xmlrpc-c/girerr.hpp \
    xmlrpc-c/girmem.hpp \
    xmlrpc-c/base.hpp \
    xmlrpc-c/timeout.hpp \
    xmlrpc-c/xml.hpp \
    xmlrpc-c/registry.hpp \
    xmlrpc-c/server_abyss.hpp \
    xmlrpc-c/packetsocket.hpp \
    xmlrpc-c/server_pstream.hpp \

    COMPAT_LINK_CMDS += $(LN_S) xmlrpc-c/oldcppwrapper.hpp XmlRpcCpp.h;
endif

HEADERINST_PREFIX = /xmlrpc-c

ifeq ($(MUST_BUILD_CLIENT),yes)
  HEADERS_TO_INSTALL += \
    xmlrpc-c/client.h \
    xmlrpc-c/transport.h \
    xmlrpc-c/client_global.h \

  COMPAT_LINK_CMDS += $(LN_S) xmlrpc-c/client.h xmlrpc_client.h;

  ifeq ($(ENABLE_CPLUSPLUS),yes)
    HEADERS_TO_INSTALL += \
      xmlrpc-c/client.hpp \
      xmlrpc-c/client_transport.hpp \
      xmlrpc-c/client_simple.hpp \

  endif
endif
ifeq ($(ENABLE_CGI_SERVER),yes)
  HEADERS_TO_INSTALL += xmlrpc-c/server_cgi.h
  COMPAT_LINK_CMDS += $(LN_S) xmlrpc-c/server_cgi.h xmlrpc_cgi.h;
endif

default: all

all:

.PHONY: install-compat-hdr
install-compat-hdr:
# Install old names of header files for backward compatibility
	cd $(DESTDIR)$(HEADERINST_DIR); \
	  rm -f xmlrpc.h xmlrpc_client.h xmlrpc_server.h xmlrpc_cgi.h \
                xmlrpc_server_abyss.h xmlrpc_server_w32httpsys.h \
	        XmlRpcCpp.h; \
	  $(COMPAT_LINK_CMDS)

.PHONY: install
install: install-common install-compat-hdr

.PHONY: clean distclean dep
clean:
distclean:
	rm -f xmlrpc-c/config.h

.PHONY: check
check:

.PHONY: dep
dep:

OMIT_CONFIG_H_RULE = Y

include $(SRCDIR)/common.mk

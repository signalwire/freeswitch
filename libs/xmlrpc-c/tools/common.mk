# We try to get Xmlrpc-c directories early in the link library search
# path to avert problems with other versions of Xmlrpc-c being in more
# general directories (such as /usr/local/lib) that are added to the
# search path by curl-config, etc.  That's why we separate the -L from
# the corresponding -l.
# 
# Note that in a properly configured system, curl-config, etc. do not
# generate -L options for general directories.

CLIENT_LDLIBS = -Lblddir/src -Lblddir/lib/libutil

CLIENT_LDLIBS += -lxmlrpc_client -lxmlrpc -lxmlrpc_util

ifeq ($(MUST_BUILD_LIBWWW_CLIENT),yes)
  CLIENT_LDLIBS += $(shell libwww-config --libs)
endif
ifeq ($(MUST_BUILD_CURL_CLIENT),yes)
  CLIENT_LDLIBS += $(shell curl-config --libs)
endif
ifeq ($(MUST_BUILD_WININET_CLIENT),yes)
  CLIENT_LDLIBS += $(shell wininet-config --libs)
endif

CLIENT_LDLIBS += $(LDLIBS_XML)

CLIENTPP_LDLIBS = -Lblddir/src/cpp
CLIENTPP_LDLIBS += -lxmlrpc_client++ -lxmlrpc_packetsocket -lxmlrpc++

include $(SRCDIR)/common.mk

ifneq ($(OMIT_LIB_RULE),Y)
srcdir/tools/lib/dumpvalue.o: FORCE
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/tools/lib/Makefile $(notdir $@) 
endif

.PHONY: install
install: install-common

.PHONY: check
check:

.PHONY: FORCE
FORCE:

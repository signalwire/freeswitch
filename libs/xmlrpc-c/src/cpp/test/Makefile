ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  SRCCPPDIR := $(call updir,$(CURDIR))
  srcDIR := $(call updir,$(SRCCPPDIR))
  SRCDIR := $(call updir,$(srcDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := src/test/cpp

include $(BLDDIR)/config.mk

PROGS = test

default: all

all: $(PROGS)

XMLRPC_C_CONFIG = $(BLDDIR)/xmlrpc-c-config.test

CXXFLAGS = $(CXXFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

LDFLAGS += $(shell $(XMLRPC_C_CONFIG) client --ldadd)

ifeq ($(MUST_BUILD_CURL_CLIENT),yes)
  LDFLAGS += $(shell curl-config --libs)
endif
ifeq ($(MUST_BUILD_LIBWWW_CLIENT),yes)
  LDFLAGS += $(shell libwww-config --libs)
endif

LDFLAGS += "-lpthread"

LDFLAGS += $(LADD)

INCLUDES = -Isrcdir/include -I$(BLDDIR) -Isrcdir -Isrcdir/lib/util/include

# This 'Makefile' dependency makes sure the symlinks get built before
# this make file is used for anything.

Makefile: srcdir

include $(SRCDIR)/common.mk


TEST_OBJS = test.o registry.o server_abyss.o server_pstream.o tools.o

ifeq ($(MUST_BUILD_CLIENT),yes)
  TEST_OBJS += testclient.o
  CLIENT_LIBS = $(LIBXMLRPC_CLIENTPP_A) $(LIBXMLRPC_CLIENT_A)
else
  TEST_OBJS += testclient_dummy.o
  CLIENT_LIBS =
endif


TEST_LIBS = \
  $(LIBXMLRPC_SERVER_ABYSSPP_A) \
  $(LIBXMLRPC_SERVER_PSTREAMPP_A) \
  $(LIBXMLRPC_SERVERPP_A) \
  $(CLIENT_LIBS) $(LIBXMLRPCPP_A) \
  $(LIBXMLRPC_CPP_A) \
  $(LIBXMLRPC_SERVER_ABYSS_A) \
  $(LIBXMLRPC_SERVER_A) \
  $(LIBXMLRPC_A) \
  $(LIBXMLRPC_ABYSS_A) \
  $(LIBXMLRPC_PACKETSOCKET_A) \
  $(LIBXMLRPC_UTIL_A) \

ifneq ($(ENABLE_LIBXML2_BACKEND),yes)
  # We're using the internal Expat XML parser
  TEST_LIBS += $(LIBXMLRPC_XMLPARSE_A) $(LIBXMLRPC_XMLTOK_A)
  LDADD_XML =
else
  LDADD_XML = $(shell xml2-config --libs)
endif

test: $(TEST_OBJS) $(TEST_LIBS)
	$(CXXLD) -o $@ $(LDFLAGS) $(LDADD_XML) $^

%.o:%.cpp
	$(CXX) -c $(INCLUDES) $(CXXFLAGS) $<

# Note the difference between 'check' and 'runtests'.  'check' means to check
# our own correctness.  'runtests' means to run the tests that check our
# parent's correctness

.PHONY: check
check:

.PHONY: runtests
runtests: test
	./test

.PHONY: install
install:

.PHONY: clean clean-local distclean
clean: clean-common clean-local
clean-local:
	rm -f $(PROGS)

distclean: clean distclean-common

.PHONY: dep
dep: dep-common

include Makefile.depend

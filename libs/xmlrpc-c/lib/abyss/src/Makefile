ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../../..
endif
SUBDIR = lib/abyss/src

include $(SRCDIR)/Makefile.config

CFLAGS = $(CFLAGS_COMMON)
CFLAGS += -D_UNIX
ifeq ($(ENABLE_ABYSS_THREADS),yes)
CFLAGS += -D_THREAD
endif
CFLAGS += 
CFLAGS +=  $(CFLAGS_PERSONAL) $(CADD)
LIBLDFLAGS = $(LDFLAGS_VERSINFO) -rpath $(LIBINST_DIR)
ifeq ($(ENABLE_ABYSS_THREADS),yes)
LIBLDFLAGS += -lpthread
endif
LIBLDFLAGS += $(LADD)

INCLUDES = -I$(SRCDIR) -I$(SRCDIR)/include -I$(SRCDIR)/lib/util/include

LTLIBRARIES_TO_INSTALL = libxmlrpc_abyss.la

default: all

.PHONY: all
all: libxmlrpc_abyss.la


ABYSS_OBJS = \
  conf.lo \
  data.lo \
  token.lo \
  trace.lo \
  file.lo \
  thread.lo \
  http.lo \
  socket.lo \
  server.lo \
  conn.lo \


libxmlrpc_abyss.la: $(ABYSS_OBJS)
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LIBLDFLAGS) $^

$(ABYSS_OBJS):%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) \
	  $(CFLAGS) $<

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

include $(SRCDIR)/Makefile.common

include Makefile.depend

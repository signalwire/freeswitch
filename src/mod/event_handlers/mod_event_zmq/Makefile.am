include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_event_zmq
ZMQ=zeromq-2.1.9

ZMQ_BASEURL=http://download.zeromq.org
ZMQ_BASEURL_ALT=http://download.zeromq.org/historic

ZMQ_DIR=$(switch_srcdir)/libs/$(ZMQ)
ZMQ_BUILDDIR=$(switch_builddir)/libs/$(ZMQ)
ZMQ_LA=$(ZMQ_BUILDDIR)/src/libzmq.la

mod_LTLIBRARIES = mod_event_zmq.la
mod_event_zmq_la_SOURCES  = mod_event_zmq.cpp
mod_event_zmq_la_CFLAGS   = $(AM_CFLAGS)
mod_event_zmq_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(ZMQ_DIR)/include -I$(switch_srcdir)/libs/libteletone/src/
mod_event_zmq_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(ZMQ_LA)
mod_event_zmq_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(ZMQ_LA)

$(ZMQ_DIR):
	$(GETLIB) $(ZMQ_BASEURL) $(ZMQ).tar.gz || $(GETLIB) $(ZMQ_BASEURL_ALT) $(ZMQ).tar.gz
	sed -e 's:AM_CONFIG_HEADER:AC_CONFIG_HEADERS:' $(ZMQ_DIR)/configure.in > $(ZMQ_DIR)/configure.in.tmp && \
		mv $(ZMQ_DIR)/configure.in.tmp $(ZMQ_DIR)/configure.in
	cd $(ZMQ_DIR) && ./autogen.sh

$(ZMQ_BUILDDIR)/Makefile: $(ZMQ_DIR)
	mkdir -p $(ZMQ_BUILDDIR)
	cd $(ZMQ_BUILDDIR) && $(DEFAULT_VARS) $(ZMQ_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(ZMQ_DIR)
	$(TOUCH_TARGET)

$(ZMQ_LA): $(ZMQ_BUILDDIR)/Makefile
	cd $(ZMQ_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

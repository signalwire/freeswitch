BASE=../../../..

ZMQ=zeromq-2.1.3

ZMQ_BASEURL=http://download.zeromq.org

ZMQ_DIR=$(switch_srcdir)/libs/$(ZMQ)
ZMQ_BUILDDIR=$(switch_builddir)/libs/$(ZMQ)
LOCAL_CFLAGS=-I$(ZMQ_DIR)/include
ZMQ_LA=$(ZMQ_BUILDDIR)/src/libzmq.la
LOCAL_LIBADD=$(ZMQ_LA)

include $(BASE)/build/modmake.rules

$(ZMQ_DIR):
	$(GETLIB) $(ZMQ_BASEURL) $(ZMQ).tar.gz
	cd $(ZMQ_DIR) && ./autogen.sh

$(ZMQ_BUILDDIR)/Makefile: $(ZMQ_DIR)
	mkdir -p $(ZMQ_DIR)
	cd $(ZMQ_BUILDDIR) && $(DEFAULT_VARS) $(ZMQ_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(ZMQ_DIR)
	$(TOUCH_TARGET)

$(ZMQ_LA): $(ZMQ_BUILDDIR)/Makefile
	cd $(ZMQ_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)


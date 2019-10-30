include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_event_zmq

mod_LTLIBRARIES = mod_event_zmq.la
mod_event_zmq_la_SOURCES  = mod_event_zmq.cpp
mod_event_zmq_la_CFLAGS   = $(AM_CFLAGS)
mod_event_zmq_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(ZMQ_DIR)/include -I$(switch_srcdir)/libs/libteletone/src/
mod_event_zmq_la_LIBADD   = $(switch_builddir)/libfreeswitch.la 
mod_event_zmq_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lzmq



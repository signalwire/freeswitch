include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_html5

LIBWEBSOCKETS_DIR=$(switch_srcdir)/libs/libwebsockets
LIBWEBSOCKETS_BUILDDIR=$(switch_builddir)/libs/libwebsockets
LIBWEBSOCKETSLA=$(LIBWEBSOCKETS_BUILDDIR)/lib/libwebsockets.la

mod_LTLIBRARIES = mod_html5.la
mod_html5_la_SOURCES = mod_html5.c
mod_html5_la_CFLAGS  = $(AM_CFLAGS) -I.
mod_html5_la_LIBADD = $(switch_builddir)/libfreeswitch.la $(LIBWEBSOCKETSLA)
mod_html5_la_LDFLAGS = -avoid-version -module -no-undefined -shared

BUILT_SOURCES = $(LIBWEBSOCKETSLA)

$(mod_html5_la_SOURCES) : $(BUILT_SOURCES)

$(LIBWEBSOCKETSLA): $(LIBWEBSOCKETS_DIR) $(LIBWEBSOCKETS_DIR)/.update
	cd $(LIBWEBSOCKETS_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)


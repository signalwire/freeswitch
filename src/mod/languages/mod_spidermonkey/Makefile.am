include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_spidermonkey

JS_DIR=$(switch_srcdir)/libs/js
JS_BUILDDIR=$(switch_builddir)/libs/js
JSLA=$(JS_BUILDDIR)/libjs.la

AM_CFLAGS  += -I. -I$(JS_DIR)/src -I$(JS_DIR)/nsprpub/dist/include/nspr -I$(JS_BUILDDIR)/src -I$(JS_BUILDDIR)/nsprpub/dist/include/nspr
AM_CFLAGS  += -DXP_UNIX  -DJS_THREADSAFE -DJS_HAS_FILE_OBJECT=1 -DJS_HAS_XML_SUPPORT=1
AM_LIBADD   = $(switch_builddir)/libfreeswitch.la $(JSLA) -lnspr4 $(JS_BUILDDIR)/nsprpub/pr/src/libnspr4.*
AM_LDFLAGS += -avoid-version -module -no-undefined -shared 
AM_LDFLAGS += -L$(JS_BUILDDIR)/nsprpub/dist/lib
BUILT_SOURCES = $(JSLA)

mod_LTLIBRARIES              = mod_spidermonkey.la 
mod_spidermonkey_la_SOURCES  = mod_spidermonkey.c
mod_spidermonkey_la_CFLAGS   = $(AM_CFLAGS)
mod_spidermonkey_la_CPPFLAGS = $(CURL_CFLAGS) $(AM_CPPFLAGS)
mod_spidermonkey_la_LIBADD   = $(AM_LIBADD)
mod_spidermonkey_la_LDFLAGS  = $(CURL_LIBS) $(AM_LDFLAGS)

mod_LTLIBRARIES             += mod_spidermonkey_curl.la
mod_spidermonkey_curl_la_SOURCES  = mod_spidermonkey_curl.c
mod_spidermonkey_curl_la_CFLAGS   = $(AM_CFLAGS)
mod_spidermonkey_curl_la_CPPFLAGS = $(CURL_CFLAGS) $(AM_CPPFLAGS)
mod_spidermonkey_curl_la_LIBADD   = $(AM_LIBADD)
mod_spidermonkey_curl_la_LDFLAGS  = $(CURL_LIBS) $(AM_LDFLAGS)

mod_LTLIBRARIES             += mod_spidermonkey_socket.la
mod_spidermonkey_socket_la_SOURCES  = mod_spidermonkey_socket.c
mod_spidermonkey_socket_la_CFLAGS   = $(AM_CFLAGS)
mod_spidermonkey_socket_la_LIBADD   = $(AM_LIBADD)
mod_spidermonkey_socket_la_LDFLAGS  = $(AM_LDFLAGS)

mod_LTLIBRARIES             += mod_spidermonkey_core_db.la 
mod_spidermonkey_core_db_la_SOURCES  = mod_spidermonkey_core_db.c
mod_spidermonkey_core_db_la_CFLAGS   = $(AM_CFLAGS)
mod_spidermonkey_core_db_la_LIBADD   = $(AM_LIBADD)
mod_spidermonkey_core_db_la_LDFLAGS  = $(AM_LDFLAGS)

mod_LTLIBRARIES             += mod_spidermonkey_teletone.la
mod_spidermonkey_teletone_la_SOURCES  = mod_spidermonkey_teletone.c
mod_spidermonkey_teletone_la_CFLAGS   = $(AM_CFLAGS) -I$(switch_srcdir)/libs/libteletone/src
mod_spidermonkey_teletone_la_LIBADD   = $(AM_LIBADD)
mod_spidermonkey_teletone_la_LDFLAGS  = $(AM_LDFLAGS)

if HAVE_ODBC
mod_LTLIBRARIES             += mod_spidermonkey_odbc.la
mod_spidermonkey_odbc_la_SOURCES  = mod_spidermonkey_odbc.c
mod_spidermonkey_odbc_la_CFLAGS   = $(AM_CFLAGS) $(ODBC_INC_FLAGS)
mod_spidermonkey_odbc_la_LIBADD   = $(AM_LIBADD)
mod_spidermonkey_odbc_la_LDFLAGS  = $(AM_LDFLAGS) -lodbc
endif

$(SOURCES): $(BUILT_SOURCES)

$(JSLA): $(JS_DIR) $(JS_DIR)/.update
	cd $(JS_BUILDDIR)/nsprpub && $(MAKE) -j1 && cd $(JS_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

install-exec-local: $(DESTDIR)$(libdir)/libjs.la

$(DESTDIR)$(libdir)/libjs.la: $(JSLA)
	cd $(JS_BUILDDIR)/nsprpub/ && $(MAKE) -j1 install && cd $(JS_BUILDDIR) && $(MAKE) install

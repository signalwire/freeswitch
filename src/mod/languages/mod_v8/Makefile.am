include $(top_srcdir)/build/modmake.rulesam

V8=v8-3.24.14

V8_DIR=$(switch_srcdir)/libs/$(V8)
V8_BUILDDIR=$(switch_builddir)/libs/$(V8)
V8SO=$(V8_DIR)/out/native/lib.target/libv8.so

CURL_DIR=$(switch_srcdir)/libs/curl
CURL_BUILDDIR=$(switch_builddir)/libs/curl
CURLLA=${switch_builddir}/libs/curl/lib/libcurl.la

MODNAME=mod_v8

AM_CFLAGS  += -I. -I./include -I$(V8_DIR)/include 
AM_CPPFLAGS += -I. -I./include -I$(V8_DIR)/include 
AM_LIBADD   = $(switch_builddir)/libfreeswitch.la -lv8 -licuuc -licui18n 
AM_LDFLAGS += -avoid-version -module -no-undefined -shared 
AM_LDFLAGS += -L$(V8_DIR)/out/native/lib.target 
BUILT_SOURCES = $(V8SO) $(LIBCURL_DEPS)

if HAVE_ODBC
AM_CFLAGS  += -DHAVE_ODBC $(ODBC_INC_FLAGS) 
AM_CPPFLAGS += -DHAVE_ODBC $(ODBC_INC_FLAGS) 
AM_LDFLAGS += -DHAVE_ODBC 
endif

mod_LTLIBRARIES            = mod_v8.la 
mod_v8_la_SOURCES  = mod_v8.cpp ./src/fscoredb.cpp ./src/fscurl.cpp ./src/fsdtmf.cpp ./src/fsevent.cpp ./src/fsfileio.cpp ./src/fsglobal.cpp ./src/fsodbc.cpp ./src/fspcre.cpp ./src/fsrequest.cpp ./src/fssession.cpp ./src/fssocket.cpp ./src/fsteletone.cpp ./src/fsxml.cpp ./src/fsfile.cpp ./src/jsbase.cpp ./src/jsmain.cpp
mod_v8_la_CFLAGS   = $(AM_CFLAGS) $(LIBCURL_CPPFLAGS) -I$(switch_srcdir)/libs/libteletone/src
mod_v8_la_CPPFLAGS = $(AM_CPPFLAGS) $(LIBCURL_CPPFLAGS) -I$(switch_srcdir)/libs/libteletone/src
mod_v8_la_LIBADD   = $(AM_LIBADD)
mod_v8_la_LDFLAGS  = $(AM_LDFLAGS) $(LIBCURL)

#mod_LTLIBRARIES                += mod_v8_skel.la
#mod_v8_skel_la_SOURCES  = mod_v8_skel.cpp
#mod_v8_skel_la_CFLAGS   = $(AM_CFLAGS) -DJSMOD_IMPORT
#mod_v8_skel_la_CPPFLAGS = $(AM_CPPFLAGS) -DJSMOD_IMPORT
#mod_v8_skel_la_LIBADD   = $(AM_LIBADD)
#mod_v8_skel_la_LDFLAGS  = $(AM_LDFLAGS)

$(SOURCES): $(BUILT_SOURCES)

$(V8_DIR):
	$(GETLIB) $(V8).tar.bz2
        
$(V8SO): $(V8_DIR)
	cd $(V8_BUILDDIR) && $(MAKE) library=shared native

$(CURLLA):
	@cd $(CURL_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

install-exec-local: $(DESTDIR)$(libdir)/libv8.so

$(DESTDIR)$(libdir)/libv8.so: $(V8SO)
	rm -f $(DESTDIR)$(libdir)/libv8.so && cp -a $(V8_DIR)/out/native/lib.target/libv8.so $(DESTDIR)$(libdir)/libv8.so
	rm -f $(DESTDIR)$(libdir)/libicuuc.so && cp -a $(V8_DIR)/out/native/lib.target/libicuuc.so $(DESTDIR)$(libdir)/libicuuc.so
	rm -f $(DESTDIR)$(libdir)/libicui18n.so && cp -a $(V8_DIR)/out/native/lib.target/libicui18n.so $(DESTDIR)$(libdir)/libicui18n.so

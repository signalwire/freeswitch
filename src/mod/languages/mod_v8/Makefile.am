include $(top_srcdir)/build/modmake.rulesam

AUTOMAKE_OPTIONS += foreign

# V8 version to use
V8_VERSION=6.1.298
V8=v8-$(V8_VERSION)

V8_DIR=$(switch_srcdir)/libs/$(V8)/v8
V8_DEPOT_TOOLS_PATH=$(switch_builddir)/libs/$(V8)
V8_BUILDDIR=$(V8_DEPOT_TOOLS_PATH)/v8

V8_LIBDIR=$(V8_BUILDDIR)/out.gn/$(V8_TARGET).release

if ISMAC
V8_LIBEXT=dylib
else
V8_LIBEXT=so
endif

# Build the dynamic lib version of V8
V8LIB=$(V8_LIBDIR)/libv8.$(V8_LIBEXT)

MODNAME=mod_v8

AM_CFLAGS    += -I. -I./include -I$(switch_srcdir)/src/mod/languages/mod_v8/include -I$(V8_DIR)/include
AM_CPPFLAGS  += -I. -I./include -I$(switch_srcdir)/src/mod/languages/mod_v8/include -I$(V8_DIR)/include -std=c++11
AM_LDFLAGS   += -avoid-version -module -no-undefined -shared

AM_LIBADD     = $(switch_builddir)/libfreeswitch.la -lv8 -lv8_libbase -lv8_libplatform
AM_LDFLAGS   += -L$(V8_LIBDIR)

BUILT_SOURCES = $(V8LIB)

if HAVE_ODBC
AM_CFLAGS   += -DHAVE_ODBC $(ODBC_INC_FLAGS)
AM_CPPFLAGS += -DHAVE_ODBC $(ODBC_INC_FLAGS)
AM_LDFLAGS  += -DHAVE_ODBC -lodbc
endif

mod_LTLIBRARIES    = mod_v8.la

mod_v8_la_SOURCES  = \
	mod_v8.cpp \
	src/jsmain.cpp \
	src/jsbase.cpp \
	src/fscoredb.cpp \
	src/fsdbh.cpp \
	src/fscurl.cpp \
	src/fsdtmf.cpp \
	src/fsevent.cpp \
	src/fsfileio.cpp \
	src/fsglobal.cpp \
	src/fsodbc.cpp \
	src/fspcre.cpp \
	src/fsrequest.cpp \
	src/fssession.cpp \
	src/fssocket.cpp \
	src/fsteletone.cpp \
	src/fsxml.cpp \
	src/fsfile.cpp \
	src/fseventhandler.cpp

mod_v8_la_CFLAGS   = $(AM_CFLAGS) -I$(switch_srcdir)/libs/libteletone/src
mod_v8_la_CPPFLAGS = $(CURL_CFLAGS) $(AM_CPPFLAGS) -I$(switch_srcdir)/libs/libteletone/src
mod_v8_la_LIBADD   = $(AM_LIBADD)
mod_v8_la_LDFLAGS  = $(CURL_LIBS) $(AM_LDFLAGS)

#mod_LTLIBRARIES        += mod_v8_skel.la
#mod_v8_skel_la_SOURCES  = mod_v8_skel.cpp
#mod_v8_skel_la_CFLAGS   = $(AM_CFLAGS) -DJSMOD_IMPORT
#mod_v8_skel_la_CPPFLAGS = $(AM_CPPFLAGS) -DJSMOD_IMPORT
#mod_v8_skel_la_LIBADD   = $(AM_LIBADD)
#mod_v8_skel_la_LDFLAGS  = $(AM_LDFLAGS)

$(SOURCES): $(BUILT_SOURCES)

$(V8LIB):
	mkdir -p $(V8_DEPOT_TOOLS_PATH)
	cd $(V8_DEPOT_TOOLS_PATH) && \
	export PATH=`pwd`/depot_tools:"$$PATH" && \
	if [ ! -d "$(V8_DEPOT_TOOLS_PATH)/depot_tools" ] ; then git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git && fetch v8 ; fi && \
	cd v8 && \
	git checkout $(V8_VERSION) && \
	gclient sync && \
	tools/dev/v8gen.py -vv $(V8_TARGET).release -- is_debug=false is_component_build=true v8_enable_i18n_support=false v8_use_external_startup_data=false && \
	ninja -C out.gn/$(V8_TARGET).release/ d8

# This is a temporary solution to force Mac OSX build to load the libraries at the right place
if ISMAC
install-exec-local: $(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT)
	install_name_tool -change @rpath/libv8.$(V8_LIBEXT) $(libdir)/libv8.$(V8_LIBEXT) .libs/mod_v8.so
	install_name_tool -change @rpath/libv8_libbase.$(V8_LIBEXT) $(libdir)/libv8_libbase.$(V8_LIBEXT) .libs/mod_v8.so
	install_name_tool -change @rpath/libv8_libplatform.$(V8_LIBEXT) $(libdir)/libv8_libplatform.$(V8_LIBEXT) .libs/mod_v8.so
else
install-exec-local: $(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT)
endif
$(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT): $(V8LIB)
	rm -f $(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT) && cp -a $(V8_LIBDIR)/libv8.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT)
	rm -f $(DESTDIR)$(libdir)/libv8_libbase.$(V8_LIBEXT) && cp -a $(V8_LIBDIR)/libv8_libbase.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libv8_libbase.$(V8_LIBEXT)
	rm -f $(DESTDIR)$(libdir)/libv8_libplatform.$(V8_LIBEXT) && cp -a $(V8_LIBDIR)/libv8_libplatform.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libv8_libplatform.$(V8_LIBEXT)

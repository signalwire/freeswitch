include $(top_srcdir)/build/modmake.rulesam

AUTOMAKE_OPTIONS = foreign

# V8 version to use
V8=v8-3.24.14

V8_DIR=$(switch_srcdir)/libs/$(V8)
V8_BUILDDIR=$(switch_builddir)/libs/$(V8)

if ISMAC
V8_LIBDIR=$(V8_DIR)/out/native
V8_LIBEXT=dylib
# Build with snapshot doesn't work for OSX right now
V8_BUILDPARAMS=snapshot=off i18nsupport=off
V8_SNAPSHOT=nosnapshot
else
V8_LIBDIR=$(V8_DIR)/out/native/lib.target
V8_LIBEXT=so
# Some gcc versions report warnings incorrectly
V8_BUILDPARAMS=strictaliasing=off werror=no i18nsupport=off
V8_SNAPSHOT=snapshot
endif

if ENABLE_STATIC_V8
# Build the static lib version of V8
if ISMAC
V8_STATIC_DIR=$(V8_DIR)/out/native
V8_ICU_STATIC_DIR=$(V8_DIR)/out/native
V8_CXXFLAGS =
else
V8_STATIC_DIR=$(V8_DIR)/out/native/obj.target/tools/gyp
V8_ICU_STATIC_DIR=$(V8_DIR)/out/native/obj.target/third_party/icu
V8_CXXFLAGS = -fPIC
endif
V8LIB=$(V8_STATIC_DIR)/libv8_base*.a
else
# Build the dynamic lib version of V8
V8_BUILDPARAMS += library=shared
V8LIB=$(V8_LIBDIR)/libv8.$(V8_LIBEXT)
endif

# Try to find the target platform for our configured CXX compiler
# Parse the result one extra time to handle different i386 platforms (i386, i486 etc)
CXX_TARGET_PLATFORM := $(shell $(CXX) -v 2>&1 | grep Target | cut '-d:' -f2 | cut '-d-' -f1 | tr -d ' ')
CXX_TARGET_PLATFORM_I386 := $(shell echo "$(CXX_TARGET_PLATFORM)" | sed 's/^\(.\{1\}\)\(.\{1\}\)/\13/')

CURL_DIR=$(switch_srcdir)/libs/curl
CURL_BUILDDIR=$(switch_builddir)/libs/curl
CURLLA=${switch_builddir}/libs/curl/lib/libcurl.la

MODNAME=mod_v8

AM_CFLAGS    += -I. -I./include -I$(V8_DIR)/include
AM_CPPFLAGS  += -I. -I./include -I$(V8_DIR)/include
AM_LDFLAGS   += -avoid-version -module -no-undefined -shared

if ENABLE_STATIC_V8
AM_LIBADD     = $(switch_builddir)/libfreeswitch.la
V8_LDFLAGS    = $(V8_STATIC_DIR)/libv8_{base*,$(V8_SNAPSHOT)*}.a $(SOLINK) #$(V8_ICU_STATIC_DIR)/libicu{i18n,uc,data}.a $(SOLINK)
else
AM_LIBADD     = $(switch_builddir)/libfreeswitch.la -lv8 #-licuuc -licui18n
AM_LDFLAGS   += -L$(V8_LIBDIR)
endif

BUILT_SOURCES = $(V8LIB) $(LIBCURL_DEPS)

if HAVE_ODBC
AM_CFLAGS   += -DHAVE_ODBC $(ODBC_INC_FLAGS)
AM_CPPFLAGS += -DHAVE_ODBC $(ODBC_INC_FLAGS)
AM_LDFLAGS  += -DHAVE_ODBC
endif

mod_LTLIBRARIES    = mod_v8.la

mod_v8_la_SOURCES  = \
	mod_v8.cpp \
	src/jsmain.cpp \
	src/jsbase.cpp \
	src/fscoredb.cpp \
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
	src/fsfile.cpp

mod_v8_la_CFLAGS   = $(AM_CFLAGS) $(LIBCURL_CPPFLAGS) -I$(switch_srcdir)/libs/libteletone/src
mod_v8_la_CPPFLAGS = $(AM_CPPFLAGS) $(LIBCURL_CPPFLAGS) -I$(switch_srcdir)/libs/libteletone/src
mod_v8_la_LIBADD   = $(AM_LIBADD)
mod_v8_la_LDFLAGS  = $(AM_LDFLAGS) $(LIBCURL) $(V8_LDFLAGS)

#mod_LTLIBRARIES        += mod_v8_skel.la
#mod_v8_skel_la_SOURCES  = mod_v8_skel.cpp
#mod_v8_skel_la_CFLAGS   = $(AM_CFLAGS) -DJSMOD_IMPORT
#mod_v8_skel_la_CPPFLAGS = $(AM_CPPFLAGS) -DJSMOD_IMPORT
#mod_v8_skel_la_LIBADD   = $(AM_LIBADD)
#mod_v8_skel_la_LDFLAGS  = $(AM_LDFLAGS)

$(SOURCES): $(BUILT_SOURCES)

$(V8_DIR):
	$(GETLIB) $(V8).tar.bz2

$(V8LIB): $(V8_DIR)
	if test "$(CXX_TARGET_PLATFORM)" = "x86_64"; then \
	   defines="v8_target_arch=x64 target_arch=x64"; \
	else \
	   if test "$(CXX_TARGET_PLATFORM)" = "arm"; then \
	      defines="v8_target_arch=arm target_arch=arm"; \
	   else \
	      if test "$(CXX_TARGET_PLATFORM_I386)" = "i386"; then \
	         defines="v8_target_arch=ia32 target_arch=ia32"; \
	      fi; \
	   fi; \
	fi; \
	cd $(V8_BUILDDIR) && CFLAGS="$(V8_CXXFLAGS)" CXXFLAGS="$(V8_CXXFLAGS)" GYPFLAGS="--no-parallel" GYP_DEFINES="$$defines" $(MAKE) $(V8_BUILDPARAMS) native

$(CURLLA):
	@cd $(CURL_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

if ENABLE_STATIC_V8
install-exec-local: $(V8LIB)
else
# This is a temporary solution to force Mac OSX build to load the libraries at the right place
if ISMAC
install-exec-local: $(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT)
	install_name_tool -change /usr/local/lib/libv8.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT) .libs/mod_v8.so
	install_name_tool -change /usr/local/lib/libicuuc.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libicuuc.$(V8_LIBEXT) .libs/mod_v8.so
	install_name_tool -change /usr/local/lib/libicui18n.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libicui18n.$(V8_LIBEXT) .libs/mod_v8.so
	install_name_tool -change /usr/local/lib/libv8.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT)
	install_name_tool -change /usr/local/lib/libicuuc.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libicuuc.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT)
	install_name_tool -change /usr/local/lib/libicui18n.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libicui18n.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT)
	install_name_tool -change /usr/local/lib/libicuuc.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libicuuc.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libicui18n.$(V8_LIBEXT)
	install_name_tool -change /usr/local/lib/libicui18n.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libicui18n.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libicui18n.$(V8_LIBEXT)
	install_name_tool -change /usr/local/lib/libicuuc.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libicuuc.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libicuuc.$(V8_LIBEXT)
else
install-exec-local: $(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT)
endif
$(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT): $(V8LIB)
	rm -f $(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT) && cp -a $(V8_LIBDIR)/libv8.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libv8.$(V8_LIBEXT)
	rm -f $(DESTDIR)$(libdir)/libicuuc.$(V8_LIBEXT) && cp -a $(V8_LIBDIR)/libicuuc.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libicuuc.$(V8_LIBEXT)
	rm -f $(DESTDIR)$(libdir)/libicui18n.$(V8_LIBEXT) && cp -a $(V8_LIBDIR)/libicui18n.$(V8_LIBEXT) $(DESTDIR)$(libdir)/libicui18n.$(V8_LIBEXT)
endif

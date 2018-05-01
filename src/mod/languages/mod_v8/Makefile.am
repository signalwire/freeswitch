include $(top_srcdir)/build/modmake.rulesam

AUTOMAKE_OPTIONS += foreign

MODNAME=mod_v8

AM_CFLAGS    += -I. -I./include -I$(switch_srcdir)/src/mod/languages/mod_v8/include $(V8FS_STATIC_CFLAGS)
AM_CPPFLAGS  += -I. -I./include -I$(switch_srcdir)/src/mod/languages/mod_v8/include $(V8FS_STATIC_CFLAGS) -std=c++11
AM_LDFLAGS   += -avoid-version -module -no-undefined -shared

AM_LIBADD     = $(switch_builddir)/libfreeswitch.la
AM_LDFLAGS   += $(V8FS_STATIC_LIBS)

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

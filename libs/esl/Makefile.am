AUTOMAKE_OPTIONS = foreign subdir-objects
SUBDIRS = . perl
MYLIB=./.libs/libesl.a
LIBS=-lncurses -lpthread -lm
LDFLAGS=-L. $(SYSTEM_LDFLAGS)
SOLINK=-shared -Xlinker -x

lib_LTLIBRARIES = libesl.la
libesl_la_CFLAGS    = $(SWITCH_AM_CFLAGS) $(PICKY) $(SYSTEM_CFLAGS) 
libesl_la_CXXFLAGS  = $(SWITCH_AM_CXXFLAGS) 
libesl_la_CPPFLAGS  = -I$(switch_srcdir)/libs/esl/src/include
libesl_la_LDFLAGS   = -version-info 1:0:0 $(AM_LDFLAGS) -static -no-undefined -L. $(SYSTEM_LDFLAGS)
libesl_la_SOURCES   = src/esl.c src/esl_event.c src/esl_threadmutex.c src/esl_config.c src/esl_json.c src/esl_buffer.c src/cJSON.c src/cJSON_Utils.c
if ENABLE_CPP
libesl_la_SOURCES += src/esl_oop.cpp
endif

$(MYLIB): libesl.la

bin_PROGRAMS = fs_cli fs_ivrd
noinst_PROGRAMS = testclient testserver testserver_fork

fs_cli_SOURCES = fs_cli.c
fs_cli_CFLAGS  = $(AM_CFLAGS) -I$(switch_srcdir)/libs/esl/src/include $(LIBEDIT_CFLAGS)
fs_cli_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS) $(LIBS) $(LIBEDIT_LIBS)
fs_cli_LDADD   = libesl.la

if DISABLE_CC
fs_cli_CFLAGS += -DDISABLE_CC
endif

testclient_SOURCES = testclient.c
testclient_CFLAGS  = $(AM_CFLAGS) -I$(switch_srcdir)/libs/esl/src/include
testclient_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS) $(LIBS)
testclient_LDADD   = libesl.la 

testserver_SOURCES = testserver.c
testserver_CFLAGS  = $(AM_CFLAGS) -I$(switch_srcdir)/libs/esl/src/include
testserver_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS) $(LIBS)
testserver_LDADD   = libesl.la 

testserver_fork_SOURCES = testserver_fork.c
testserver_fork_CFLAGS  = $(AM_CFLAGS) -I$(switch_srcdir)/libs/esl/src/include
testserver_fork_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS) $(LIBS)
testserver_fork_LDADD   = libesl.la 

fs_ivrd_SOURCES = ivrd.c
fs_ivrd_CFLAGS  = $(AM_CFLAGS) -I$(switch_srcdir)/libs/esl/src/include
fs_ivrd_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS) $(LIBS)
fs_ivrd_LDADD   = libesl.la 

reswig:	swigclean
	$(MAKE) -C perl reswig
	$(MAKE) -C php reswig
	$(MAKE) -C lua reswig
	$(MAKE) -C python reswig
	$(MAKE) -C ruby reswig
	$(MAKE) -C java reswig
	$(MAKE) -C managed reswig

swigclean: clean
	$(MAKE) -C perl swigclean
	$(MAKE) -C php swigclean
	$(MAKE) -C lua swigclean
	$(MAKE) -C python swigclean
	$(MAKE) -C ruby swigclean
	$(MAKE) -C java swigclean
	$(MAKE) -C managed swigclean

perlmod:
	$(MAKE)	-C perl

phpmod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C php 

luamod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" LUA_CFLAGS="$(LUA_CFLAGS)" LUA_LIBS="$(LUA_LIBS)" -C lua

pymod: $(MYLIB)
	$(MAKE) PYTHON=python MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C python

py3mod: $(MYLIB)
	$(MAKE) PYTHON=python3 MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C python

tclmod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C tcl

rubymod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C ruby

javamod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C java

managedmod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C managed

perlmod-install:
	$(MAKE)	-C perl install

phpmod-install: phpmod
	$(MAKE) -C php install

pymod-install: pymod
	$(MAKE) PYTHON=python -C python install

py3mod-install: py3mod
	$(MAKE) PYTHON=python3 -C python install

rubymod-install: rubymod
	$(MAKE) -C ruby install

everymod: perlmod phpmod luamod pymod rubymod javamod managedmod

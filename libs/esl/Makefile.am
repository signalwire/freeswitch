AUTOMAKE_OPTIONS = foreign subdir-objects
MYLIB=./.libs/libesl.a
LIBS=-lncurses -lpthread -lm
LDFLAGS=-L. $(SYSTEM_LDFLAGS)
SOLINK=-shared -Xlinker -x

lib_LTLIBRARIES = libesl.la
libesl_la_CFLAGS    = $(SWITCH_AM_CFLAGS) $(PICKY) $(SYSTEM_CFLAGS) 
libesl_la_CXXFLAGS  = $(SWITCH_AM_CXXFLAGS) 
libesl_la_CPPFLAGS  = -I$(switch_srcdir)/libs/esl/src/include
libesl_la_LDFLAGS   = -version-info 1:0:0 $(AM_LDFLAGS) -static -no-undefined -L. $(SYSTEM_LDFLAGS)
libesl_la_SOURCES   = src/esl.c src/esl_event.c src/esl_threadmutex.c src/esl_config.c src/esl_json.c src/esl_buffer.c
if ENABLE_CPP
libesl_la_SOURCES += src/esl_oop.cpp
endif

$(MYLIB): libesl.la

bin_PROGRAMS = fs_cli ivrd
noinst_PROGRAMS = testclient testserver testserver_fork

fs_cli_SOURCES = fs_cli.c
fs_cli_CFLAGS  = $(AM_CFLAGS) -I$(switch_srcdir)/libs/esl/src/include 
fs_cli_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS) $(LIBS) 
fs_cli_LDADD   = libesl.la 

if HAVE_LIBEDIT
fs_cli_CFLAGS += -DHAVE_EDITLINE -I$(switch_srcdir)/libs/libedit/src
fs_cli_LDADD  += ../libedit/src/.libs/libedit.a $(TINFO_LIBS)
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

ivrd_SOURCES = ivrd.c
ivrd_CFLAGS  = $(AM_CFLAGS) -I$(switch_srcdir)/libs/esl/src/include
ivrd_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS) $(LIBS)
ivrd_LDADD   = libesl.la 

if HAVE_PERL
perldir = $(PERL_SITEDIR)
perl_LTLIBRARIES = ESL.la
ESL_la_SOURCES   = perl/esl_wrap.cpp perl/perlxsi.c
ESL_la_CFLAGS    = $(CC_CFLAGS) $(CFLAGS) -I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS) $(PERL_CFLAGS)
ESL_la_CXXFLAGS  = -I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS) $(CXXFLAGS) -w $(PERL_INC)
ESL_la_LDFLAGS   = -avoid-version -module -no-undefined -shared $(PERL_LDFLAGS)
ESL_la_LIBADD    = libesl.la

perlmod: ESL.la

install-data-local: perlmod-install

perlmod-install: install-perlLTLIBRARIES
	install -m 755 perl/ESL.pm $(PERL_SITEDIR)
	install -d -m 755 ESL $(PERL_SITEDIR)/ESL
	install -m 755 perl/ESL/* $(PERL_SITEDIR)/ESL
endif

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

phpmod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C php 

luamod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C lua

pymod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C python

tclmod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C tcl

rubymod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C ruby

javamod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C java

managedmod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS)" CXXFLAGS="-I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C managed

phpmod-install: phpmod
	$(MAKE) -C php install

pymod-install: pymod
	$(MAKE) -C python install

everymod: perlmod phpmod luamod pymod rubymod javamod managedmod

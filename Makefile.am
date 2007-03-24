EXTRA_DIST =
SUBDIRS = . src
AUTOMAKE_OPTS = foreign
NAME=freeswitch
PREFIX=$(prefix)

AM_CFLAGS   = $(SWITCH_AM_CFLAGS)
AM_CPPFLAGS = $(SWITCH_AM_CXXFLAGS)
AM_LDFLAGS  = $(SWITCH_AM_LDFLAGS)
BASE        = $(switch_srcdir)
OSARCH=`uname -s`

LIBTOOL=`if test -z "$(VERBOSE)" ; then echo $(SHELL) $(switch_builddir)/quiet_libtool ;else echo $(switch_builddir)/libtool;  fi;`

LTINSTALL = $(LIBTOOL) --mode=install $(INSTALL)
TOUCH_TARGET=if test -f "$@" ; then touch "$@" ; fi ;
AM_MAKEFLAGS=`test -n "$(VERBOSE)" || echo -s`

if CRASHPROT
AM_CFLAGS += -DCRASH_PROT
endif

libfreeswitch_la_SOURCES	= \
src/switch_apr.c \
src/switch_buffer.c \
src/switch_caller.c \
src/switch_channel.c \
src/switch_console.c \
src/switch_core.c \
src/switch_core_db.c\
src/switch_loadable_module.c \
src/switch_utils.c \
src/switch_event.c \
src/switch_resample.c \
src/switch_regex.c\
src/switch_rtp.c\
src/switch_ivr.c \
src/switch_stun.c\
src/switch_log.c\
src/switch_xml.c\
libs/libteletone/src/libteletone_detect.c\
libs/libteletone/src/libteletone_generate.c


library_includedir      = $(PREFIX)/include

library_include_HEADERS = \
src/include/switch_am_config.h\
src/include/switch.h\
src/include/switch_apr.h\
src/include/switch_buffer.h\
src/include/switch_caller.h\
src/include/switch_channel.h\
src/include/switch_console.h\
src/include/switch_core.h\
src/include/switch_core_db.h\
src/include/switch_event.h\
src/include/switch_frame.h\
src/include/switch_ivr.h\
src/include/switch_loadable_module.h\
src/include/switch_module_interfaces.h\
src/include/switch_platform.h\
src/include/switch_resample.h\
src/include/switch_regex.h\
src/include/switch_types.h\
src/include/switch_utils.h\
src/include/switch_rtp.h\
src/include/switch_stun.h\
src/include/switch_log.h\
src/include/switch_xml.h\
libs/libteletone/src/libteletone_detect.h\
libs/libteletone/src/libteletone_generate.h\
libs/libteletone/src/libteletone.h



CORE_CFLAGS     = `$(switch_srcdir)/libs/apr/apr-1-config --cflags --cppflags --includes`
CORE_CFLAGS    += `$(switch_srcdir)/libs/apr-util/apu-1-config --includes`
CORE_CFLAGS    += -I$(switch_srcdir)/libs/sqlite
CORE_CFLAGS    += -I$(switch_srcdir)/libs/pcre
CORE_CFLAGS    += -I$(switch_srcdir)/libs/srtp/include
CORE_CFLAGS    += -I$(switch_srcdir)/libs/srtp/crypto/include
CORE_CFLAGS    += -I$(switch_srcdir)/libs/libresample/include
CORE_CFLAGS    += -I$(switch_srcdir)/libs/libteletone/src

CORE_LIBS      = libs/apr/libapr-1.la libs/apr-util/libaprutil-1.la
CORE_LIBS     += libs/sqlite/libsqlite3.la libs/pcre/libpcre.la
CORE_LIBS     += libs/srtp/libsrtp.la libs/libresample/libresample.la

lib_LTLIBRARIES	          = libfreeswitch.la
libfreeswitch_la_CFLAGS   = $(CORE_CFLAGS) $(AM_CFLAGS)
libfreeswitch_la_LDFLAGS  = -version-info 1:0:0 $(AM_LDFLAGS)
libfreeswitch_la_LIBADD  = $(CORE_LIBS)
nodist_libfreeswitch_la_SOURCES = src/include/switch_version.h

MOD_LINK       = $(BASE)/libfreeswitch.la

CLEANFILES     = src/include/switch_version.h
BUILT_SOURCES  = src/include/switch_version.h

bin_PROGRAMS = freeswitch
freeswitch_SOURCES = src/switch.c
nodist_freeswitch_SOURCES = src/include/switch_version.h
freeswitch_CFLAGS = $(AM_CFLAGS)
freeswitch_LDFLAGS = $(AM_LDFLAGS) -rpath $(libdir)
freeswitch_LDADD = libfreeswitch.la libs/apr/libapr-1.la

$(libfreeswitch_la_SOURCES): $(CORE_LIBS) $(switch_builddir)/quiet_libtool

$(switch_builddir)/quiet_libtool: $(switch_builddir)/libtool
	@cat libtool | sed -e 's|$$show "$$command"|if test -z "$$suppress_output" ; then $$show "Compiling $$srcfile ..." ; fi|' > quiet_libtool

install-data-local:
	@echo Installing $(NAME)
	@for x in conf mod db log log/xml_cdr bin scripts htdocs grammar ; do \
		$(mkinstalldirs) $(DESTDIR)$(prefix)/$$x ; \
	 done
	@if [ ! -f $(DESTDIR)$(PREFIX)/conf/freeswitch.xml ] ; then \
		$(INSTALL) conf/*.xml $(DESTDIR)$(PREFIX)/conf ; \
	 fi

.version:
	touch .version

src/include/switch_version.h: $(top_srcdir)/src/include/switch_version.h.in .version $(libfreeswitch_la_SOURCES) $(library_include_HEADERS)
	@if test ! -f .noversion ; then \
	  force=0 ; \
	  version=`svnversion . -n || echo hacked` ; \
	  oldversion=`cat .version 2>/dev/null || echo "0"` ; \
	  test ! -f src/include/switch_version.h || grep "@SVN_VERSION@" src/include/switch_version.h && force=1 ; \
	  if test "$$oldversion" != "$$version" || test $$force = 1 ; then \
	    cat src/include/switch_version.h.in | sed "s/@SVN_VERSION@/$$version/g" > src/include/switch_version.h ; \
	    echo $$version > .version ; \
	  fi ; \
	fi ;

update:
	@if test -d .svn ; then \
	  test ! -f .version || rm -f .version ; \
	  echo Updating... ; \
	  svn update ; \
	else \
	  echo "This source directory is not an svn working copy" ; \
	fi

.nodepends:
	touch .nodepends

nodepends: .nodepends

yesdepends:
	rm .nodepends

libs/apr/libapr-1.la: libs/apr libs/apr/.update
	@cd libs/apr && $(MAKE)
	@$(TOUCH_TARGET)

libs/apr-util/libaprutil-1.la: libs/apr-util libs/apr-util/.update
	@cd libs/apr-util && $(MAKE)
	@$(TOUCH_TARGET)

libs/sqlite/libsqlite3.la: libs/sqlite libs/sqlite/.update
	@cd libs/sqlite && $(MAKE)
	@$(TOUCH_TARGET)

libs/pcre/libpcre.la: libs/pcre libs/pcre/.update
	@cd libs/pcre && $(MAKE)
	@$(TOUCH_TARGET)

libs/srtp/libsrtp.la: libs/srtp libs/srtp/.update
	@cd libs/srtp && $(MAKE)
	@$(TOUCH_TARGET)

libs/libresample/libresample.la: libs/libresample libs/libresample/.update
	@cd libs/libresample && $(MAKE)
	@$(TOUCH_TARGET)

core:  libfreeswitch.la  

install_core: install-libLTLIBRARIES

core_install: install_core

everything: install

installall: install

sure: clean modwipe uninstall installall

wayclean: clean

modules: libfreeswitch.la
	@cd src/mod && $(MAKE) $(AM_MAKEFLAGS)

install_mod: libfreeswitch.la
	@cd src/mod && $(MAKE) $(AM_MAKEFLAGS) install

mod_install: install_mod

uninstall_mod:
	@cd src/mod && $(MAKE) $(AM_MAKEFLAGS) uninstall

mod_uninstall: uninstall_mod

modclean:
	@cd src/mod && $(MAKE) $(AM_MAKEFLAGS) clean

modwipe:
	rm -f $(PREFIX)/mod/*.${DYNAMIC_LIB_EXTEN}

dox:
	cd docs && doxygen $(PWD)/docs/Doxygen.conf

eclean: clean
	rm -f `find . -type f -name \*~`
	rm -f `find . -type f -name \.*~`
	rm -f `find . -type f -name \#\*`
	rm -f `find . -type f -name \.\#\*`
	rm -f `find . -type f -name core\*`
	rm -f *.tar *.tgz *.gz 

megaclean: eclean modclean
	rm -f `find ./libs -name \*.la`

EXTRA_DIST =
SUBDIRS = . src/mod
AUTOMAKE_OPTS = foreign
NAME=freeswitch
PREFIX=$(prefix)

AM_CFLAGS   = $(SWITCH_AM_CFLAGS)
AM_CPPFLAGS = $(SWITCH_AM_CXXFLAGS)
AM_LDFLAGS  = $(SWITCH_AM_LDFLAGS)
BASE        = $(switch_srcdir)
OSARCH=$(shell uname -s)

LIBTOOL=$(switch_builddir)/libtool
LTINSTALL = $(LIBTOOL) --mode=install $(INSTALL)

if CRASHPROT
AM_CFLAGS += -DCRASH_PROT
endif

libfreeswitch_la_SOURCES	= \
src/switch_apr.c \
src/switch_buffer.c \
src/switch_caller.c \
src/switch_channel.c \
src/switch_config.c \
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
src/switch_xml.c

library_includedir      = $(PREFIX)/include

library_include_HEADERS = \
src/include/switch_am_config.h\
src/include/switch.h\
src/include/switch_apr.h\
src/include/switch_buffer.h\
src/include/switch_caller.h\
src/include/switch_channel.h\
src/include/switch_config.h\
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
src/include/switch_version.h\
src/include/switch_stun.h\
src/include/switch_log.h\
src/include/switch_xml.h

CORE_CFLAGS     = $(shell $(switch_srcdir)/libs/apr/apr-1-config --cflags --cppflags --includes)
CORE_CFLAGS    += $(shell $(switch_srcdir)/libs/apr-util/apu-1-config --includes)
CORE_CFLAGS    += -I$(switch_srcdir)/libs/sqlite
CORE_CFLAGS    += -I$(switch_srcdir)/libs/pcre
CORE_CFLAGS    += -I$(switch_srcdir)/libs/srtp/include
CORE_CFLAGS    += -I$(switch_srcdir)/libs/srtp/crypto/include
CORE_CFLAGS    += -I$(switch_srcdir)/libs/libresample/include
CORE_CFLAGS    += -I$(switch_srcdir)/libs/libteletone/src

CORE_LIBS      = libs/apr/libapr-1.la libs/apr-util/libaprutil-1.la
CORE_LIBS     += libs/sqlite/libsqlite3.la libs/pcre/libpcre.la
CORE_LIBS     += libs/srtp/libsrtp.la libs/libresample/libresample.la
CORE_LIBS     += libs/libteletone/libteletone.la

lib_LTLIBRARIES	          = libfreeswitch.la
libfreeswitch_la_CFLAGS   = $(CORE_CFLAGS) $(AM_CFLAGS)
libfreeswitch_la_LDFLAGS  = -version-info 1:0:0 $(AM_LDFLAGS)
libfreeswitch_la_LIBADD  = $(CORE_LIBS)
nodist_libfreeswitch_la_SOURCES = src/include/switch_version.h

$(libfreeswitch_la_SOURCES): deps

MOD_LINK       = $(BASE)/libfreeswitch.la

CLEANFILES     = src/include/switch_version.h

bin_PROGRAMS =  freeswitch
freeswitch_SOURCES =    src/switch.c\
                        src/include/switch_version.h
freeswitch_CFLAGS = $(AM_CFLAGS)
freeswitch_LDADD = ${MOD_LINK}

install-data-local:
	@echo Installing $(NAME)
	@for x in conf mod db log log/xml_cdr bin scripts htdocs grammar ; do \
		$(mkinstalldirs) $(DESTDIR)$(prefix)/$$x ; \
	 done
	@if [ ! -f $(DESTDIR)$(PREFIX)/conf/freeswitch.xml ] ; then \
		$(INSTALL) conf/*.xml $(DESTDIR)$(PREFIX)/conf ; \
	 fi
	@if [ -f .libs/$(NAME) ] ; then \
		$(INSTALL) .libs/$(NAME) $(DESTDIR)$(PREFIX)/bin/$(NAME) ; \
	 else \
		$(INSTALL) ./$(NAME) $(DESTDIR)$(PREFIX)/bin/$(NAME) ;\
	 fi

version: Makefile src/include/switch_version.h
	MAKE=$(MAKE) ./build/checkversion.sh

newversion: Makefile
	MAKE=$(MAKE) ./build/checkversion.sh force
	$(MAKE)

.nodepends:
	touch .nodepends

nodepends: .nodepends

yesdepends:
	rm .nodepends

sqlite:
	cd libs/sqlite && $(MAKE)

apr:
	cd libs/apr && $(MAKE)

apr-util:
	cd libs/apr-util && $(MAKE)

pcre:
	cd libs/pcre && $(MAKE)

srtp:
	cd libs/srtp && $(MAKE)

resample:
	cd libs/libresample && $(MAKE)

teletone:
	cd libs/libteletone && $(MAKE)

deps: version sqlite apr apr-util pcre srtp resample teletone

core: deps libfreeswitch.la  

everything: install

installall: install

sure: clean modwipe uninstall installall

wayclean: clean

modules: libfreeswitch.la
	$(MAKE) -C src/mod
install_mod: libfreeswitch.la
	$(MAKE) -C src/mod install
modclean:
	$(MAKE) -C src/mod clean

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
	rm -fr .depend `find . -name .complete`

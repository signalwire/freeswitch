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

.DEFAULT:
	@target=`echo $@ | sed -e 's|^.*-||'`; \
         if test "$$target" = "install"; then $(MAKE) $(AM_MAKEFLAGS) core_install; else $(MAKE) $(AM_MAKEFLAGS) core ; fi;
	@cd src/mod && $(MAKE) $(AM_MAKEFLAGS) $@

libfreeswitch_la_SOURCES	= \
src/switch_apr.c \
src/switch_buffer.c \
src/switch_caller.c \
src/switch_channel.c \
src/switch_console.c \
src/switch_core_media_bug.c \
src/switch_core_timer.c \
src/switch_core_asr.c \
src/switch_core_event_hook.c \
src/switch_core_speech.c \
src/switch_core_memory.c \
src/switch_core_codec.c \
src/switch_core_file.c \
src/switch_core_hash.c \
src/switch_core_sqldb.c \
src/switch_core_session.c \
src/switch_core_directory.c \
src/switch_core_state_machine.c \
src/switch_core_io.c \
src/switch_core_rwlock.c \
src/switch_core_port_allocator.c \
src/switch_core.c \
src/switch_scheduler.c \
src/switch_core_db.c\
src/switch_loadable_module.c \
src/switch_utils.c \
src/switch_event.c \
src/switch_resample.c \
src/switch_regex.c\
src/switch_rtp.c\
src/switch_ivr_bridge.c \
src/switch_ivr_originate.c \
src/switch_ivr_async.c \
src/switch_ivr_play_say.c \
src/switch_ivr_menu.c \
src/switch_ivr.c \
src/switch_stun.c\
src/switch_log.c\
src/switch_xml.c\
src/switch_config.c\
src/switch_time.c\
libs/stfu/stfu.c\
src/switch_cpp.cpp\
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
src/include/switch_core_event_hook.h\
src/include/switch_scheduler.h\
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
src/include/switch_cpp.h\
libs/libteletone/src/libteletone_detect.h\
libs/libteletone/src/libteletone_generate.h\
libs/libteletone/src/libteletone.h\
src/include/switch_odbc.h

CORE_CFLAGS     = `$(switch_srcdir)/libs/apr/apr-1-config --cflags --cppflags --includes`
CORE_CFLAGS    += `$(switch_srcdir)/libs/apr-util/apu-1-config --includes`
CORE_CFLAGS    += -I$(switch_srcdir)/libs/stfu
CORE_CFLAGS    += -I$(switch_srcdir)/libs/sqlite
CORE_CFLAGS    += -I$(switch_srcdir)/libs/pcre
CORE_CFLAGS    += -I$(switch_srcdir)/libs/srtp/include
CORE_CFLAGS    += -I$(switch_srcdir)/libs/srtp/crypto/include
CORE_CFLAGS    += $(RESAMPLE_CFLAGS)

CORE_LIBS      = libs/apr/libapr-1.la libs/apr-util/libaprutil-1.la
CORE_LIBS     += libs/sqlite/libsqlite3.la libs/pcre/libpcre.la
CORE_LIBS     += libs/srtp/libsrtp.la $(RESAMPLE_LIB)

lib_LTLIBRARIES	          = libfreeswitch.la
libfreeswitch_la_CFLAGS   = $(CORE_CFLAGS) $(AM_CFLAGS)
libfreeswitch_la_LDFLAGS  = -version-info 1:0:0 $(AM_LDFLAGS)
libfreeswitch_la_LIBADD  = $(CORE_LIBS)
libfreeswitch_la_DEPENDENCIES = $(BUILT_SOURCES)
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


if ADD_LIBEDIT
CORE_CFLAGS    += -Ilibs/libedit/src -DSWITCH_HAVE_LIBEDIT
CORE_LIBS      += libs/libedit/src/.libs/libedit.a
freeswitch_LDADD += -lcurses
endif

if ADD_ODBC
CORE_CFLAGS    += -DSWITCH_HAVE_ODBC
libfreeswitch_la_SOURCES += src/switch_odbc.c
libfreeswitch_la_LDFLAGS     += -lodbc
endif




libs/libedit/Makefile:
	cd libs/libedit && ./configure CFLAGS="-D_XOPEN_SOURCE=600 -D_BSD_SOURCE=1" --disable-shared --with-pic
libs/libedit/src/.libs/libedit.a: libs/libedit/Makefile
	cd libs/libedit && $(MAKE)

$(libfreeswitch_la_SOURCES): $(CORE_LIBS) $(switch_builddir)/quiet_libtool modules.conf

$(switch_builddir)/quiet_libtool: $(switch_builddir)/libtool
	@cat libtool | sed -e 's|$$show "$$command"|if test -z "$$suppress_output" ; then $$show "Compiling $$srcfile ..." ; fi|' > quiet_libtool

install-data-local:
	@echo Installing $(NAME)
	@for x in conf conf/dialplan conf/directory conf/sip_profiles mod db log log/xml_cdr bin scripts htdocs grammar ; do \
		$(mkinstalldirs) $(DESTDIR)$(prefix)/$$x ; \
	 done
	@if [ ! -f $(DESTDIR)$(PREFIX)/conf/freeswitch.xml ] ; then \
		$(INSTALL) conf/*.xml $(DESTDIR)$(PREFIX)/conf ; \
		$(INSTALL) conf/sip_profiles/*.xml $(DESTDIR)$(PREFIX)/conf/sip_profiles ; \
		$(INSTALL) conf/dialplan/*.xml $(DESTDIR)$(PREFIX)/conf/dialplan ; \
		$(INSTALL) conf/directory/*.xml $(DESTDIR)$(PREFIX)/conf/directory ; \
	 fi

.version:
	touch .version

src/include/switch_version.h: $(top_srcdir)/src/include/switch_version.h.in .version $(libfreeswitch_la_SOURCES) $(library_include_HEADERS)
	@have_version=1 ; \
	force=0 ; \
	grep "@SWITCH_VERSION_REVISION@" src/include/switch_version.h.in > /dev/null  && have_version=0 ; \
	test ! -f src/include/switch_version.h || grep "@SWITCH_VERSION_REVISION@" src/include/switch_version.h > /dev/null && force=1 ; \
	if test $$have_version = 1 ; then \
	  cat src/include/switch_version.h.in > src/include/switch_version.h ; \
	  touch .version ; \
	else \
	  version=`svnversion . -n || echo hacked` ; \
	  oldversion=`cat .version 2>/dev/null || echo "0"` ; \
	  if test "$$oldversion" != "$$version" || test $$force = 1 ; then \
	    cat src/include/switch_version.h.in | sed "s/@SWITCH_VERSION_REVISION@/$$version/g" > src/include/switch_version.h ; \
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

libs/apr-util/libaprutil-1.la: libs/apr-util libs/apr-util/.update libs/apr/libapr-1.la
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

if USE_INTREE_RESAMPLE
libs/libresample/libresample.la: libs/libresample libs/libresample/.update
	@cd libs/libresample && $(MAKE)
	@$(TOUCH_TARGET)
endif

core:
	$(MAKE) $(AM_MAKEFLAGS) libfreeswitch.la

clean_core: clean-libLTLIBRARIES

install_core: install-libLTLIBRARIES

core_install: install_core

everything: install

installall: install

sure: clean modwipe uninstall
	$(MAKE) -j install

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

modules.conf:
	if test -f $@; then touch $@; else cp build/modules.conf.in $@ ;fi

$(OUR_MODULES):
	@set fnord $$MAKEFLAGS; amf=$$2; \
	(cd src/mod && $(MAKE) $(AM_MAKEFLAGS) $@) \
	   || case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
	test -z "$$fail"

# Special targets

coffee me_a_sandwich me_a_sammich:
	@echo "Who do you think I am?  Make it yourself!"

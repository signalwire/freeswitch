EXTRA_DIST =
SUBDIRS = . src build
AUTOMAKE_OPTS = foreign
NAME=freeswitch
PREFIX=$(prefix)

AM_CFLAGS   = $(SWITCH_AM_CFLAGS)
AM_CPPFLAGS = $(SWITCH_AM_CXXFLAGS)
AM_LDFLAGS  = $(SWITCH_AM_LDFLAGS)
BASE        = $(switch_srcdir)
OSARCH=`uname -s`

DEFAULT_SOUNDS=en-us-callie-8000

.DEFAULT:
	@target=`echo $@ | sed -e 's|^.*-||'`; \
	 target_prefix=`echo $@ | sed -e 's|-.*$$||'`; \
	 sound_perfix=`echo $@ | sed -e 's|-.*||'`; \
	 full_sound_dir=`echo $@ | sed -e 's|^sounds||' | sed -e 's|^-||' | sed -e 's|-install$$||'`; \
	 test ! -z $$full_sound_dir || full_sound_dir=`echo $(DEFAULT_SOUNDS)`; \
	 soundfile=`echo freeswitch-sounds-$$full_sound_dir-$(PACKAGE_VERSION).tar.gz`; \
         if test "$$target" = "install"; then $(MAKE) $(AM_MAKEFLAGS) core_install; else $(MAKE) $(AM_MAKEFLAGS) core ; fi; \
         if test "$$target_prefix" = "sounds"; then \
	   if test "$$target" = "install"; then $(GETSOUNDS) $$soundfile $(PREFIX)/sounds/; else $(GETSOUNDS) $$soundfile ; fi; \
	 else \
	   cd src/mod && $(MAKE) $(AM_MAKEFLAGS) $@ ;\
	 fi

sounds: sounds-en-us-callie-8000
sounds-install: sounds-en-us-callie-8000-install
moh: sounds-music-8000
moh-install: sounds-music-8000-install

hd-sounds: sounds-en-us-callie-8000 sounds-en-us-callie-16000
hd-sounds-install: sounds-en-us-callie-8000-install sounds-en-us-callie-16000-install
hd-moh: sounds-music-8000 sounds-music-16000
hd-moh-install: sounds-music-8000-install sounds-music-16000-install

uhd-sounds: sounds-en-us-callie-8000 sounds-en-us-callie-16000 sounds-en-us-callie-32000
uhd-sounds-install: sounds-en-us-callie-8000-install sounds-en-us-callie-16000-install sounds-en-us-callie-32000-install
uhd-moh: sounds-music-8000 sounds-music-16000 sounds-music-32000
uhd-moh-install: sounds-music-8000-install sounds-music-16000-install sounds-music-32000-install

libdir = @libdir@
bindir = @bindir@
includedir = @includedir@
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
src/switch_xml.cpp\
src/switch_config.c\
src/switch_time.c\
libs/stfu/stfu.c\
src/switch_cpp.cpp\
src/g711.c\
src/switch_pcm.c\
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
src/include/switch_config.h\
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

CORE_LIBS      = libs/apr-util/libaprutil-1.la libs/apr/libapr-1.la
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
freeswitch_CFLAGS = $(AM_CFLAGS) $(CORE_CFLAGS)
freeswitch_LDFLAGS = $(AM_LDFLAGS) -rpath $(libdir)
freeswitch_LDADD = libfreeswitch.la libs/apr/libapr-1.la
if ADD_LIBEDIT
freeswitch_CFLAGS    += -Ilibs/libedit/src -DSWITCH_HAVE_LIBEDIT
freeswitch_LDADD      += libs/libedit/src/.libs/libedit.a
endif


if ADD_LIBEDIT
CORE_CFLAGS    += -Ilibs/libedit/src -DSWITCH_HAVE_LIBEDIT
CORE_LIBS      += libs/libedit/src/.libs/libedit.a
endif

if ADD_ODBC
CORE_CFLAGS    += -DSWITCH_HAVE_ODBC
libfreeswitch_la_SOURCES += src/switch_odbc.c
libfreeswitch_la_LDFLAGS     += -Wl,-lodbc
endif

bin_SCRIPTS = scripts/gentls_cert scripts/fsxs


libs/libedit/src/.libs/libedit.a:
	cd libs/libedit && $(MAKE)

$(libfreeswitch_la_SOURCES): $(CORE_LIBS) $(switch_builddir)/quiet_libtool modules.conf

$(switch_builddir)/quiet_libtool: $(switch_builddir)/libtool
	@cat libtool | sed -e 's|$$show "$$command"|if test -z "$$suppress_output" ; then $$show "Compiling $$srcfile ..." ; fi|' > quiet_libtool

samples-conf:
	test -d $(DESTDIR)$(prefix)/conf || $(mkinstalldirs) $(DESTDIR)$(prefix)/conf
	test -d $(DESTDIR)$(prefix)/conf/dialplan/extensions || $(mkinstalldirs) $(DESTDIR)$(prefix)/conf/dialplan/extensions
	for conffile in `find conf -name \*.xml && find conf -name \*.conf && find conf -name \*.tpl  && find conf -name \*.ttml && find conf -name mime.types` ; do \
		dir=`echo $$conffile | sed -e 's|/[^/]*$$||'`; \
		filename=`echo $$conffile | sed -e 's|^.*/||'`; \
		test -d $(DESTDIR)$(PREFIX)/$$dir || $(mkinstalldirs) $(DESTDIR)$(prefix)/$$dir ; \
		test -f $(DESTDIR)$(PREFIX)/$$dir/$$filename || \
		test -f $(DESTDIR)$(PREFIX)/$$dir/$$filename.noload || \
			$(INSTALL) -m 644 $$dir/$$filename $(DESTDIR)$(PREFIX)/$$dir; \
	done

samples-htdocs: 
	test -d $(DESTDIR)$(prefix)/htdocs || $(mkinstalldirs) $(DESTDIR)$(prefix)/htdocs
	for htdocsfile in `find htdocs -name \* | grep -v .svn` ; do \
		dir=`echo $$htdocsfile | sed -e 's|/[^/]*$$||'`; \
		filename=`echo $$htdocsfile | sed -e 's|^.*/||'`; \
		test -d $(DESTDIR)$(PREFIX)/$$dir || $(mkinstalldirs) $(DESTDIR)$(prefix)/$$dir ; \
		test -f $(DESTDIR)$(PREFIX)/$$dir/$$filename || $(INSTALL) -m 644 $$dir/$$filename $(DESTDIR)$(PREFIX)/$$dir 2>/dev/null;  \
	done

samples: samples-conf samples-htdocs

install-data-local:
	@echo Installing $(NAME)
	@for x in mod db log log/xml_cdr bin scripts grammar recordings ; do \
		$(mkinstalldirs) $(DESTDIR)$(prefix)/$$x ; \
	 done
	$(mkinstalldirs) $(DESTDIR)@modinstdir@
	test -d $(DESTDIR)$(prefix)/conf || $(MAKE) samples-conf
	test -d $(DESTDIR)$(prefix)/htdocs || $(MAKE) samples-htdocs

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
	@cd libs/sqlite && $(MAKE) CFLAGS="$(SWITCH_AM_CFLAGS)"
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

core-clean: clean_core

core-install: core_install

clean_core: clean-libLTLIBRARIES

install_core: install-libLTLIBRARIES

core_install: install_core

everything: install

installall: install

up: clean
	svn update
	$(MAKE) -j core
	$(MAKE) -j modules
	$(MAKE) install

libs/openzap/Makefile:
	cd libs/openzap && autoconf
	cd libs/openzap && ./configure

update-clean: clean modwipe uninstall libs/openzap/Makefile
	cd libs/sofia-sip && $(MAKE) clean
	cd libs/openzap && $(MAKE) clean

sure: update-clean all

speedy-sure: update-clean
	$(MAKE) -j core
	cd libs/sofia-sip && $(MAKE) -j
	$(MAKE) -j modules

current: update-clean
	svn update
	$(MAKE) all
	$(MAKE) install

speedy-current: update-clean
	svn update
	$(MAKE) speedy-sure
	$(MAKE) install

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

scripts/fsxs: scripts/fsxs.in
	@echo creating fsxs
	@sed -e "s,@MODULES_DIR\@,@modinstdir@," \
	    -e "s,@LIB_DIR\@,@libdir@," \
	    -e "s,@BIN_DIR\@,@bindir@," \
	    -e "s,@INC_DIR\@,@includedir@," \
	    -e "s,@CFG_DIR\@,$(PREFIX)/conf," \
	    -e "s,@DB_DIR\@,$(PREFIX)/db," \
	    -e "s,@PREFIX\@,$(PREFIX)," \
	    -e "s,@CC\@,$(CC)," \
	    -e "s,@LD\@,$(CC)," \
	    -e "s,@INSTALL\@,$(INSTALL)," \
	    -e "s,@MKINSTALLDIRS\@,$(mkdir_p)," \
	    \
	    -e "s|@CFLAGS\@|$(CFLAGS) `$(switch_srcdir)/libs/apr/apr-1-config --cflags --cppflags`|" \
	    -e "s|@INCLUDES\@|-I$(PREFIX)/include|" \
	    -e "s|@SOLINK\@|$(SOLINK)|" \
	    -e "s|@LDFLAGS\@|-L$(PREFIX)/lib|" \
	    -e "s|@LIBS\@|`$(switch_srcdir)/libs/apr/apr-1-config --libs` `$(switch_srcdir)/libs/apr-util/apu-1-config --libs`|" \
	    scripts/fsxs.in > scripts/fsxs

eclean: clean
	rm -f `find . -type f -name \*~`
	rm -f `find . -type f -name \.*~`
	rm -f `find . -type f -name \#\*`
	rm -f `find . -type f -name \.\#\*`
	rm -f `find . -type f -name core\*`
	rm -f *.tar *.tgz *.gz 

megaclean: eclean modclean
	rm -f `find ./libs -name \*.la`

libclean:
	@for file in `ls ./libs`; do pushd "./libs/"$$file; make clean; rm -f .built; popd; done

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

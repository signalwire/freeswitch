EXTRA_DIST =
SUBDIRS = . src build
AUTOMAKE_OPTIONS = foreign subdir-objects
NAME = freeswitch

if SYSTEM_APR
AM_LIBAPR_CFLAGS := $(shell apr-1-config --cflags)
AM_LIBAPR_CPPFLAGS := $(shell apr-1-config --cppflags --includes)
AM_LIBAPR_LDFLAGS := $(shell apr-1-config --ldflags)
AM_LIBAPR_LIBS := $(shell apr-1-config \--libs)
AM_LIBAPR_LINKLIBTOOL := $(shell apr-1-config \--link-libtool)
else
AM_LIBAPR_CFLAGS := $(shell ./libs/apr/apr-1-config --cflags)
AM_LIBAPR_CPPFLAGS := $(shell ./libs/apr/apr-1-config --cppflags --includes)
AM_LIBAPR_LDFLAGS := $(shell ./libs/apr/apr-1-config --ldflags)
AM_LIBAPR_LIBS := $(subst $(switch_builddir)/,,$(shell ./libs/apr/apr-1-config \--libs))
endif
if SYSTEM_APRUTIL
AM_LIBAPU_CPPFLAGS := $(shell apu-1-config --includes)
AM_LIBAPU_LDFLAGS := $(shell apu-1-config --ldflags)
AM_LIBAPU_LIBS := $(shell apu-1-config \--libs)
AM_LIBAPU_LINKLIBTOOL := $(shell apu-1-config \--link-libtool)
else
AM_LIBAPU_CPPFLAGS := $(shell ./libs/apr-util/apu-1-config --includes)
AM_LIBAPU_LDFLAGS := $(shell ./libs/apr-util/apu-1-config --ldflags)
AM_LIBAPU_LIBS := $(subst $(switch_builddir)/,,$(shell ./libs/apr-util/apu-1-config \--libs))
endif

AM_CFLAGS   = $(SWITCH_AM_CFLAGS) $(SWITCH_ANSI_CFLAGS) 
AM_CPPFLAGS = $(SWITCH_AM_CXXFLAGS) -I$(switch_srcdir)/libs/sofia-sip/libsofia-sip-ua/sdp -I$(switch_srcdir)/libs/sofia-sip/libsofia-sip-ua/su -I$(switch_builddir)/libs/sofia-sip/libsofia-sip-ua/su
AM_LDFLAGS  = $(SWITCH_AM_LDFLAGS) $(AM_LIBAPR_LDFLAGS) $(AM_LIBAPU_LDFLAGS)

DEFAULT_SOUNDS=en-us-callie-8000

.DEFAULT: $(switch_builddir)/modules.conf
	@target=`echo $@ | sed -e 's|^.*-||'`; \
	 target_prefix=`echo $@ | sed -e 's|-.*$$||'`; \
	 sound_perfix=`echo $@ | sed -e 's|-.*||'`; \
	 moh_version=`cat $(switch_srcdir)/build/moh_version.txt`;\
	 full_sound_dir=`echo $@ | sed -e 's|^sounds||' | sed -e 's|^-||' | sed -e 's|-install$$||'`; \
	 test ! -z $$full_sound_dir || full_sound_dir=`echo $(DEFAULT_SOUNDS)`; \
	 base_sound_dir=`echo $$full_sound_dir | sed -e 's|-[^-]*000$$||' ` ;\
	 sounds_version=`grep $$base_sound_dir $(switch_srcdir)/build/sounds_version.txt | cut -d ' '  -f2`;\
	 soundfile=`echo freeswitch-sounds-$$full_sound_dir-$$moh_version.tar.gz`; \
	 echo $$full_sound_dir | grep  music  >/dev/null || soundfile=`echo freeswitch-sounds-$$full_sound_dir-$$sounds_version.tar.gz`; \
         if test "$$target_prefix" = "sounds"; then \
	   if test "$$target" = "install"; then \
	      $(GETSOUNDS) $$soundfile $(DESTDIR)$(soundsdir)/;\
	   else \
	      $(GETSOUNDS) $$soundfile ; \
	   fi; \
	 else \
	   if test "$$target" = "install"; then \
	      $(MAKE) $(AM_MAKEFLAGS) core_install && cd src/mod && $(MAKE) $(AM_MAKEFLAGS) $@ ; \
	   else \
	      $(MAKE) $(AM_MAKEFLAGS) core && cd src/mod && $(MAKE) $(AM_MAKEFLAGS) $@ ;\
	   fi; \
	 fi

sounds: sounds-en-us-callie-8000
sounds-install: sounds-en-us-callie-8000-install
sounds-ru: sounds-ru-RU-elena-8000
sounds-ru-install: sounds-ru-RU-elena-8000-install
moh: sounds-music-8000
moh-install: sounds-music-8000-install

hd-sounds: sounds sounds-en-us-callie-16000
hd-sounds-install: sounds-install sounds-en-us-callie-16000-install
hd-sounds-ru: sounds-ru sounds-ru-RU-elena-16000
hd-sounds-ru-install: sounds-ru-install sounds-ru-RU-elena-16000-install
hd-moh: moh sounds-music-16000
hd-moh-install: moh-install sounds-music-16000-install

uhd-sounds: hd-sounds sounds-en-us-callie-32000
uhd-sounds-install: hd-sounds-install sounds-en-us-callie-32000-install
uhd-sounds-ru: hd-sounds-ru sounds-ru-RU-elena-32000
uhd-sounds-ru-install: hd-sounds-ru-install sounds-ru-RU-elena-32000-install
uhd-moh: hd-moh sounds-music-32000
uhd-moh-install: hd-moh-install sounds-music-32000-install

cd-sounds: uhd-sounds sounds-en-us-callie-48000
cd-sounds-install: uhd-sounds-install sounds-en-us-callie-48000-install
cd-sounds-ru: uhd-sounds-ru sounds-ru-RU-elena-48000
cd-sounds-ru-install: uhd-sounds-ru-install sounds-ru-RU-elena-48000-install
cd-moh: uhd-moh sounds-music-48000
cd-moh-install: uhd-moh-install sounds-music-48000-install

# This directory's subdirectories are mostly independent; you can cd
# into them and run `make' without going through this Makefile.
# To change the values of `make' variables: instead of editing Makefiles,
# (1) if the variable is set in `config.status', edit `config.status'
#     (which will cause the Makefiles to be regenerated when you run `make');
# (2) otherwise, pass the desired values on the `make' command line.
all-recursive: libfreeswitch.la
clean-recusive: clean_core
install-recursive: install-libLTLIBRARIES install-binPROGRAMS

CORE_CFLAGS  = $(AM_LIBAPR_CFLAGS) $(AM_LIBAPR_CPPFLAGS)
CORE_CFLAGS += $(AM_LIBAPU_CPPFLAGS)
CORE_CFLAGS += -I$(switch_srcdir)/libs/libtpl-1.5/src
CORE_CFLAGS += -I$(switch_srcdir)/libs/srtp/include
CORE_CFLAGS += -I$(switch_srcdir)/libs/srtp/crypto/include -Ilibs/srtp/crypto/include
CORE_CFLAGS += -I$(switch_builddir)/libs/spandsp/src -I$(switch_srcdir)/libs/spandsp/src 
CORE_CFLAGS += -I$(switch_builddir)/libs/tiff-4.0.2/libtiff -I$(switch_srcdir)/libs/tiff-4.0.2/libtiff

APR_LIBS   = $(AM_LIBAPU_LIBS) $(AM_LIBAPR_LIBS)
CORE_LIBS=
if SYSTEM_APR
CORE_LIBS += $(AM_LIBAPR_LINKLIBTOOL)
else
CORE_LIBS += libs/apr/libapr-1.la
endif
if SYSTEM_APRUTIL
CORE_LIBS += $(AM_LIBAPU_LINKLIBTOOL)
else
CORE_LIBS +=  libs/apr-util/libaprutil-1.la
endif

if ENABLE_SRTP
CORE_CFLAGS += -DENABLE_SRTP
CORE_LIBS += libs/srtp/libsrtp.la
endif

MOD_LINK      = $(switch_srcdir)/libfreeswitch.la
CLEANFILES    = src/include/switch_version.h src/include/switch_swigable_cpp.h
BUILT_SOURCES = src/include/switch_version.h src/include/switch_swigable_cpp.h

if HAVE_ODBC
CORE_CFLAGS += -DSWITCH_HAVE_ODBC $(ODBC_INC_FLAGS)
endif

if ENABLE_TIMERFD_WRAPPER
CORE_CFLAGS += -DTIMERFD_WRAP
endif

##
## libfreeswitch
##
noinst_LTLIBRARIES        = libfreeswitch_spandsp.la
libfreeswitch_spandsp_la_SOURCES = libs/spandsp/src/plc.c libs/spandsp/src/alloc.c libs/spandsp/src/bit_operations.c
libfreeswitch_spandsp_la_CFLAGS  = -Ilibs/spandsp/src $(CORE_CFLAGS) $(AM_CFLAGS)
CORE_LIBS+=libfreeswitch_spandsp.la
lib_LTLIBRARIES	         = libfreeswitch.la
libfreeswitch_la_CFLAGS  = $(CORE_CFLAGS) $(SQLITE_CFLAGS) $(CURL_CFLAGS) $(PCRE_CFLAGS) $(SPEEX_CFLAGS) $(LIBEDIT_CFLAGS) $(AM_CFLAGS)
libfreeswitch_la_LDFLAGS = -version-info 1:0:0 $(AM_LDFLAGS) $(PLATFORM_CORE_LDFLAGS) -no-undefined
libfreeswitch_la_LIBADD  = $(CORE_LIBS) $(APR_LIBS) $(SQLITE_LIBS) $(CURL_LIBS) $(PCRE_LIBS) $(SPEEX_LIBS) $(LIBEDIT_LIBS) $(PLATFORM_CORE_LIBS)
libfreeswitch_la_DEPENDENCIES = $(BUILT_SOURCES)

if HAVE_ODBC
libfreeswitch_la_LDFLAGS += $(ODBC_LIB_FLAGS)
endif

if ENABLE_ZRTP
CORE_CFLAGS += -I$(switch_srcdir)/libs/libzrtp/third_party/bgaes
CORE_CFLAGS += -I$(switch_srcdir)/libs/libzrtp/third_party/bnlib
CORE_CFLAGS += -isystem $(switch_srcdir)/libs/libzrtp/include
ZRTP_LDFLAGS = -L$(switch_srcdir)/libs/libzrtp/third_party/bnlib
ZRTP_LDFLAGS += -L$(switch_srcdir)/libs/libzrtp
ZRTP_LIBS = -lbn -lzrtp
libfreeswitch_la_LDFLAGS += $(ZRTP_LDFLAGS)
libfreeswitch_la_LIBADD += $(ZRTP_LIBS)
CORE_LIBS += libs/libzrtp/libzrtp.a
LIBS += libs/libzrtp/third_party/bnlib/libbn.a
endif

library_includedir      = $(includedir)
library_include_HEADERS = \
	src/include/switch_am_config.h \
	src/include/switch.h \
	src/include/switch_apr.h \
	src/include/switch_buffer.h \
	src/include/switch_caller.h \
	src/include/switch_channel.h \
	src/include/switch_console.h \
	src/include/switch_core_event_hook.h \
	src/include/switch_scheduler.h \
	src/include/switch_core.h \
	src/include/switch_core_media.h \
	src/include/switch_core_db.h \
	src/include/switch_mprintf.h \
	src/include/switch_config.h \
	src/include/switch_event.h \
	src/include/switch_frame.h \
	src/include/switch_ivr.h \
	src/include/switch_dso.h \
	src/include/switch_loadable_module.h \
	src/include/switch_module_interfaces.h \
	src/include/switch_platform.h \
	src/include/switch_resample.h \
	src/include/switch_regex.h \
	src/include/switch_types.h \
	src/include/switch_utils.h \
	src/include/switch_rtp.h \
	src/include/switch_rtcp_frame.h \
	src/include/switch_stun.h \
	src/include/switch_nat.h \
	src/include/switch_log.h \
	src/include/switch_xml.h \
	src/include/switch_xml_config.h \
	src/include/switch_cpp.h \
	src/include/switch_curl.h \
	src/include/switch_json.h \
	src/include/switch_stfu.h \
	libs/libteletone/src/libteletone_detect.h \
	libs/libteletone/src/libteletone_generate.h \
	libs/libteletone/src/libteletone.h \
	libs/libtpl-1.5/src/tpl.h \
	src/include/switch_limit.h \
	src/include/switch_odbc.h \
	src/include/switch_pgsql.h \
	src/include/switch_hashtable.h

nodist_libfreeswitch_la_SOURCES = \
	src/include/switch_frame.h \
	src/include/switch_swigable_cpp.h \
	src/include/switch_version.h

libfreeswitch_la_SOURCES = \
	src/switch_apr.c \
	src/switch_buffer.c \
	src/switch_caller.c \
	src/switch_channel.c \
	src/switch_console.c \
	src/switch_mprintf.c \
	src/switch_core_media_bug.c \
	src/switch_core_timer.c \
	src/switch_core_asr.c \
	src/switch_core_event_hook.c \
	src/switch_core_speech.c \
	src/switch_core_memory.c \
	src/switch_core_codec.c \
	src/switch_core_file.c \
	src/switch_core_cert.c \
	src/switch_core_hash.c \
	src/switch_core_sqldb.c \
	src/switch_core_session.c \
	src/switch_core_directory.c \
	src/switch_core_state_machine.c \
	src/switch_core_io.c \
	src/switch_core_rwlock.c \
	src/switch_core_port_allocator.c \
	src/switch_core.c \
	src/switch_version.c \
	src/switch_core_media.c \
	src/switch_sdp.c \
	src/switch_scheduler.c \
	src/switch_core_db.c \
	src/switch_dso.c \
	src/switch_loadable_module.c \
	src/switch_utils.c \
	src/switch_event.c \
	src/switch_resample.c \
	src/switch_regex.c \
	src/switch_rtp.c \
	src/switch_ivr_bridge.c \
	src/switch_ivr_originate.c \
	src/switch_ivr_async.c \
	src/switch_ivr_play_say.c \
	src/switch_ivr_say.c \
	src/switch_ivr_menu.c \
	src/switch_ivr.c \
	src/switch_stun.c \
	src/switch_nat.c \
	src/switch_log.c \
	src/switch_xml.c \
	src/switch_xml_config.c \
	src/switch_config.c \
	src/switch_time.c \
	src/switch_odbc.c \
	src/switch_pgsql.c \
	src/switch_limit.c \
	src/g711.c \
	src/switch_pcm.c \
	src/switch_speex.c \
	src/switch_profile.c \
	src/switch_json.c \
	src/switch_curl.c \
	src/switch_hashtable.c\
	src/switch_stfu.c \
	libs/libtpl-1.5/src/tpl.c \
	libs/libteletone/src/libteletone_detect.c \
	libs/libteletone/src/libteletone_generate.c \
	libs/miniupnpc/miniwget.c \
	libs/miniupnpc/minixml.c \
	libs/miniupnpc/igd_desc_parse.c \
	libs/miniupnpc/minisoap.c \
	libs/miniupnpc/miniupnpc.c \
	libs/miniupnpc/upnpreplyparse.c \
	libs/miniupnpc/upnpcommands.c \
	libs/miniupnpc/minissdpc.c \
	libs/miniupnpc/upnperrors.c \
	libs/libnatpmp/natpmp.c \
	libs/libnatpmp/getgateway.c

if ENABLE_CPP
libfreeswitch_la_SOURCES += src/switch_cpp.cpp
endif

$(libfreeswitch_la_SOURCES): $(CORE_LIBS) $(switch_builddir)/modules.conf  

src/include/switch_swigable_cpp.h: $(switch_srcdir)/src/include/switch_cpp.h
	cat $(switch_srcdir)/src/include/switch_cpp.h | perl $(switch_srcdir)/build/strip.pl > $(switch_srcdir)/src/include/switch_swigable_cpp.h
#	$(CC) -E $(switch_srcdir)/src/include/switch_cpp.h \
#		-I$(switch_srcdir)/src/include -I$(switch_srcdir)/libs/libteletone/src \
#		-DSWITCH_DECLARE_CLASS= -DSWITCH_DECLARE\(x\)=x -DSWITCH_DECLARE_CONSTRUCTOR= \
#		-DSWITCH_DECLARE_NONSTD\(x\)=x 2>/dev/null | grep -v "^#" > src/include/switch_swigable_cpp.h

##
## Applications
##
bin_PROGRAMS = freeswitch fs_cli fs_ivrd tone2wav fs_encode

##
## fs_cli ()
##
fs_cli_SOURCES = libs/esl/src/esl.c libs/esl/src/esl_config.c libs/esl/src/esl_event.c \
	libs/esl/src/esl_threadmutex.c libs/esl/fs_cli.c libs/esl/src/esl_json.c libs/esl/src/esl_buffer.c
fs_cli_CFLAGS  = $(AM_CFLAGS) -I$(switch_srcdir)/libs/esl/src/include $(LIBEDIT_CFLAGS)
fs_cli_LDFLAGS = $(AM_LDFLAGS) -lpthread $(ESL_LDFLAGS) -lm $(LIBEDIT_LIBS)


##
## fs_encode ()
##
fs_encode_SOURCES = src/fs_encode.c
fs_encode_CFLAGS  = $(AM_CFLAGS)
fs_encode_LDFLAGS = $(AM_LDFLAGS)
fs_encode_LDADD   = libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)

if HAVE_ODBC
fs_encode_LDADD += $(ODBC_LIB_FLAGS)
endif


##
## tone2wav ()
##
tone2wav_SOURCES = src/tone2wav.c
tone2wav_CFLAGS  = $(AM_CFLAGS)
tone2wav_LDFLAGS = $(AM_LDFLAGS)
tone2wav_LDADD   = libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)

if HAVE_ODBC
tone2wav_LDADD += $(ODBC_LIB_FLAGS)
endif


##
## fs_ivrd ()
##
fs_ivrd_SOURCES = libs/esl/src/esl.c libs/esl/src/esl_config.c libs/esl/src/esl_event.c \
	libs/esl/src/esl_threadmutex.c libs/esl/ivrd.c libs/esl/src/esl_json.c libs/esl/src/esl_buffer.c
fs_ivrd_CFLAGS  = $(AM_CFLAGS) -I$(switch_srcdir)/libs/esl/src/include 
fs_ivrd_LDFLAGS = $(AM_LDFLAGS) -lpthread $(ESL_LDFLAGS) -lm

##
## freeswitch ()
##
nodist_freeswitch_SOURCES = src/include/switch_version.h
freeswitch_SOURCES = src/switch.c
freeswitch_CFLAGS  = $(AM_CFLAGS) $(CORE_CFLAGS)
freeswitch_LDFLAGS = $(AM_LDFLAGS) -lpthread -rpath $(libdir)
freeswitch_LDADD   = libfreeswitch.la libs/apr/libapr-1.la

if HAVE_ODBC
freeswitch_LDADD += $(ODBC_LIB_FLAGS)
endif


##
## Scripts
##
bin_SCRIPTS = scripts/gentls_cert scripts/fsxs

scripts/fsxs: scripts/fsxs.in
	@echo creating fsxs
	@sed -e "s,@MODULES_DIR\@,@modulesdir@," \
	    -e "s,@LIB_DIR\@,@libdir@," \
	    -e "s,@BIN_DIR\@,@bindir@," \
	    -e "s,@INC_DIR\@,@includedir@," \
	    -e "s,@CFG_DIR\@,@sysconfdir@," \
	    -e "s,@DB_DIR\@,@dbdir@," \
	    -e "s,@PREFIX\@,@prefix@," \
	    -e "s,@CC\@,$(CC)," \
	    -e "s,@LD\@,$(CC)," \
	    -e "s,@INSTALL\@,$(INSTALL)," \
	    -e "s,@MKINSTALLDIRS\@,$(mkdir_p)," \
	    \
	    -e "s|@CFLAGS\@|$(CFLAGS) `./libs/apr/apr-1-config --cflags --cppflags`|" \
	    -e "s|@INCLUDES\@|-I$(prefix)/include|" \
	    -e "s|@SOLINK\@|$(SOLINK)|" \
	    -e "s|@LDFLAGS\@|-L$(prefix)/lib|" \
	    -e "s|@LIBS\@||" \
	    $(top_srcdir)/scripts/fsxs.in > scripts/fsxs

##
## misc
##

pkgconfigdir   = @pkgconfigdir@
pkgconfig_DATA = build/freeswitch.pc

$(switch_builddir)/modules.conf:
	if test -f $@; then touch $@; else cp $(switch_srcdir)/build/modules.conf.in $@ ;fi

$(OUR_MODULES): $(switch_builddir)/modules.conf libfreeswitch.la
	@set fnord $$MAKEFLAGS; amf=$$2; \
	(cd src/mod && $(MAKE) $(AM_MAKEFLAGS) $@) \
	   || case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
	test -z "$$fail"

build/print_git_revision: build/print_git_revision.c
	$(CC) -o $@ $<

src/switch_version.lo: src/include/switch_version.h

src/include/switch_version.h: src/include/switch_version.h.in Makefile build/print_git_revision $(libfreeswitch_la_SOURCES) $(library_include_HEADERS)
	@cat $< > $@; \
	if [ -d .git ] && [ -n "$$(which git)" ]; then \
	  xver="$$(./build/print_git_revision)"; \
	  xhver="$$(./build/print_git_revision -h)"; \
	  sed \
	    -e "/#define *SWITCH_VERSION_REVISION[^a-zA-Z0-9_]/{s/\"\([^\"]*\)\"/\"\1$$xver\"/;}" \
	    -e "/#define *SWITCH_VERSION_REVISION_HUMAN[^a-zA-Z0-9_]/{s/\"\([^\"]*\)\"/\"\1$$xhver\"/;}" \
	    $< > $@; \
	fi;

##
## Dependency targets
##
libs/libedit/src/.libs/libedit.a:
	cd libs/libedit && $(MAKE)

libs/libzrtp/libzrtp.a:
	cd libs/libzrtp && $(MAKE)

libs/sofia-sip/Makefile:
	cd libs/sofia-sip && sh ./configure.gnu --prefix=$(prefix)

libs/sofia-sip/libsofia-sip-ua/sdp/.libs/libsdp.a libs/sofia-sip/libsofia-sip-ua/su/.libs/libsu.a: libs/sofia-sip/.update libs/sofia-sip/Makefile
	@cd libs/sofia-sip && $(MAKE) noop
	@cd libs/sofia-sip && $(MAKE) SOFIA_CFLAGS="$(SWITCH_AM_CFLAGS)"
	@$(TOUCH_TARGET)

libs/apr/Makefile: libs/apr/Makefile.in libs/apr/config.status libs/apr libs/apr/.update
	@cd libs/apr && ./config.status
	@$(TOUCH_TARGET)

libs/apr/libapr-1.la: libs/apr/Makefile libs/apr/.update
	@if [ $(MAKELEVEL) = 0 -o -z "`echo "$(MAKEARGS)" | grep  "j"`" ] ; then touch $(switch_srcdir)/src/include/switch.h; cd libs/apr && $(MAKE) $(MFLAGS) && touch libapr-1.la; fi

libs/apr-util/libaprutil-1.la: libs/apr/libapr-1.la libs/apr-util libs/apr-util/.update
	@if [ $(MAKELEVEL) = 0 -o -z "`echo "$(MAKEARGS)" | grep  "j"`" ] ; then touch $(switch_srcdir)/src/include/switch.h; cd libs/apr-util && $(MAKE) $(MFLAGS) && touch libaprutil-1.la; fi

SRTP_SRC =	libs/srtp/srtp/srtp.c libs/srtp/srtp/ekt.c libs/srtp/crypto/cipher/cipher.c libs/srtp/crypto/cipher/null_cipher.c \
		libs/srtp/crypto/cipher/aes.c libs/srtp/crypto/cipher/aes_icm.c \
		libs/srtp/crypto/cipher/aes_cbc.c \
		libs/srtp/crypto/hash/null_auth.c libs/srtp/crypto/hash/sha1.c \
		libs/srtp/crypto/hash/hmac.c libs/srtp/crypto/hash/auth.c \
		libs/srtp/crypto/math/datatypes.c libs/srtp/crypto/math/stat.c \
		libs/srtp/crypto/kernel/crypto_kernel.c libs/srtp/crypto/kernel/alloc.c \
		libs/srtp/crypto/kernel/key.c \
		libs/srtp/crypto/rng/prng.c libs/srtp/crypto/rng/ctr_prng.c \
		libs/srtp/crypto/kernel/err.c libs/srtp/crypto/rng/rand_source.c \
		libs/srtp/crypto/replay/rdb.c libs/srtp/crypto/replay/rdbx.c libs/srtp/crypto/replay/ut_sim.c 

libs/srtp/libsrtp.la: libs/srtp libs/srtp/.update $(SRTP_SRC)
	touch $(switch_srcdir)/src/include/switch.h
	@cd libs/srtp && $(MAKE)
	@$(TOUCH_TARGET)

##
## helper targets
##
yaml-files:
	@echo `mkdir $(DESTDIR)$(sysconfdir)/yaml 2>/dev/null`
	$(INSTALL) -m 644 $(switch_srcdir)/conf/default/yaml/*.yaml $(DESTDIR)$(sysconfdir)/yaml

vm-sync:
	test -d $(DESTDIR)$(sysconfdir) || $(mkinstalldirs) $(DESTDIR)$(sysconfdir)
	test -d $(DESTDIR)$(sysconfdir)/lang || $(mkinstalldirs) $(DESTDIR)$(sysconfdir)/lang
	test -d $(DESTDIR)$(sysconfdir)/lang/en || $(mkinstalldirs) $(DESTDIR)$(sysconfdir)/lang/en
	test -d $(DESTDIR)$(sysconfdir)/lang/en/demo || $(mkinstalldirs) $(DESTDIR)$(sysconfdir)/lang/en/demo
	test -d $(DESTDIR)$(sysconfdir)/lang/en/vm || $(mkinstalldirs) $(DESTDIR)$(sysconfdir)/lang/en/vm
	$(INSTALL) -m 644 $(switch_srcdir)/conf/vanilla/lang/en/vm/* $(DESTDIR)$(sysconfdir)/lang/en/vm
	$(INSTALL) -m 644 $(switch_srcdir)/conf/vanilla/lang/en/demo/* $(DESTDIR)$(sysconfdir)/lang/en/demo

config-%:
	test -d $(DESTDIR)$(sysconfdir) || $(mkinstalldirs) $(DESTDIR)$(sysconfdir)
	for conffile in `cd $(switch_srcdir)/conf/$* && find . -name \*.xml && find . -name \*.conf && find . -name \*.tpl  && find . -name \*.ttml && find . -name mime.types` ; do \
		dir=`echo $$conffile | sed -e 's|^\.||' | sed -e 's|/[^/]*$$||'`; \
		filename=`echo $$conffile | sed -e 's|^\.||' | sed -e 's|^.*/||'`; \
		test -d $(DESTDIR)$(sysconfdir)$$dir || $(mkinstalldirs) $(DESTDIR)$(sysconfdir)$$dir ; \
		test -f $(DESTDIR)$(sysconfdir)$$dir/$$filename || \
		test -f $(DESTDIR)$(sysconfdir)$$dir/$$filename.noload || \
			$(INSTALL) -m 644 $(switch_srcdir)/conf/$*/$$dir/$$filename $(DESTDIR)$(sysconfdir)$$dir; \
	done

samples-conf: config-vanilla

samples-htdocs: 
	test -d $(DESTDIR)$(htdocsdir) || $(mkinstalldirs) $(DESTDIR)$(htdocsdir)
	for htdocsfile in `cd $(switch_srcdir)/htdocs && find . -type f -name \* | sed -e 's|^\.||'` ; do \
		dir=`echo $$htdocsfile | sed -e 's|/[^/]*$$||'`; \
		filename=`echo $$htdocsfile | sed -e 's|^.*/||'`; \
		test -d $(DESTDIR)$(htdocsdir)$$dir || $(mkinstalldirs) $(DESTDIR)$(htdocsdir)$$dir ; \
		test -f $(DESTDIR)$(htdocsdir)$$dir/$$filename || $(INSTALL) -m 644 $(switch_srcdir)/htdocs/$$dir/$$filename $(DESTDIR)$(htdocsdir)$$dir 2>/dev/null;  \
	done


samples: samples-conf samples-htdocs

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(modulesdir)

install-data-local:
	@echo Installing $(NAME)
	@for x in $(modulesdir) $(runtimedir) $(dbdir) $(logfiledir) $(logfiledir)/xml_cdr $(bindir) $(scriptdir) $(recordingsdir) $(grammardir); do \
		$(mkinstalldirs) $(DESTDIR)$$x ; \
	 done
	test -d $(DESTDIR)$(sysconfdir) || $(MAKE) samples-conf
	test -d $(DESTDIR)$(htdocsdir) || $(MAKE) samples-htdocs

is-scm:
	@if [ ! -d .git ] ; then \
		echo ; echo ; \
		echo "*****************************************************************************************************" ; \
		echo "You cannot update a release tarball without a git tree.  Please clone FreeSWITCH as so:              " ; \
		echo "  git clone git://git.freeswitch.org/freeswitch.git                                                  " ; \
		echo "*****************************************************************************************************" ; \
		echo ; echo ; \
		exit 1; \
	fi 

update: is-scm
	@if test -d .git ; then \
	  echo "Pulling updates..." ; \
	  git pull ; \
	else \
	  echo "This source directory is not a git tree." ; \
	fi

.nodepends:
	touch .nodepends

nodepends: .nodepends

yesdepends:
	rm .nodepends

core: $(switch_builddir)/modules.conf src/include/switch_version.h $(CORE_LIBS) 
	$(MAKE) $(AM_MAKEFLAGS) libfreeswitch.la

distclean: clean

core-clean: clean_core

core-install: core_install

clean_core: clean-libLTLIBRARIES
	rm -f $(libfreeswitch_la_OBJECTS)
	rm -f $(libfreeswitch_spandsp_la_OBJECTS)
	rm -f `echo $(libfreeswitch_la_OBJECTS) | sed -e's|.lo|.o|g'`
	rm -f `echo $(libfreeswitch_spandsp_la_OBJECTS) | sed -e's|.lo|.o|g'`

install_core: install-libLTLIBRARIES

core_install: install_core

everything: install

up: is-scm clean
	$(MAKE) update
	$(MAKE) -j core
	$(MAKE) -j modules
	$(MAKE) install

sync: is-scm
	$(MAKE) update
	$(MAKE) install

speedy-sync: is-scm
	$(MAKE) update
	$(MAKE) -j install

libs/openzap/Makefile:
	cd libs/openzap && autoconf
	cd libs/openzap && ./configure

version:
	git log -1  | head -3

reinstall: modwipe uninstall install

pristine:
	git clean -fdx
	git reset --hard

update-clean: clean libs/openzap/Makefile python-reconf lua-reconf spandsp-reconf srtp-reconf
	cd libs/sofia-sip && $(MAKE) clean
	cd libs/openzap && $(MAKE) clean
	cd libs/portaudio && $(MAKE) clean
	cd libs/esl && $(MAKE) clean
	cd libs/srtp && $(MAKE) clean

swigall:
	@echo reswigging all
	sh $(switch_srcdir)/build/swigall.sh

sndfile-reconf:
	cd libs/libsndfile && autoreconf
	cd libs/libsndfile && ./config.status --recheck
	cd libs/libsndfile && ./config.status

tiff-reconf:
	cd libs/tiff-4.0.2 && autoreconf -fi
	cd libs/tiff-4.0.2 && sh ./configure.gnu
	cd libs/tiff-4.0.2 && make

python-reconf:
	rm -f src/mod/languages/mod_python/Makefile
	./config.status

pa-reconf:
	cd libs/portaudio && $(MAKE) clean
	cd libs/portaudio && sh ./configure.gnu
	$(MAKE) mod_portaudio-clean

srtp-reconf:
	cd libs/srtp && $(MAKE) clean
	cd libs/srtp && sh ./configure.gnu

oz-reconf:
	cd libs/openzap && $(MAKE) clean
	cd libs/openzap && autoreconf -fi
	cd libs/openzap && sh ./configure.gnu --prefix=$(prefix)
	$(MAKE) mod_openzap-clean

iks-reconf:
	cd libs/iksemel && $(MAKE) clean
	cd libs/iksemel && autoreconf -fi
	cd libs/iksemel && sh ./configure.gnu --prefix=$(prefix)
	$(MAKE) mod_dingaling-clean

spandsp-reconf: tiff-reconf
	cd libs/spandsp && $(MAKE) clean || echo
	cd libs/spandsp && autoreconf -fi
	cd libs/spandsp && sh ./configure.gnu --prefix=$(prefix)
	cd libs/spandsp && $(MAKE)

sofia-reconf:
	cd libs/sofia-sip && sh ./autogen.sh
	cd libs/sofia-sip && $(MAKE) clean
	cd libs/sofia-sip && ./configure --prefix=$(prefix) --with-pic --with-glib=no --disable-shared

lua-reconf:
	cd src/mod/languages/mod_lua/lua && make clean
	cd src/mod/languages/mod_lua && make clean

cluecon:
	@clear
	@echo Thank you for updating. This is going to take a while so relax.
	@echo Now would be a good time to register for ClueCon!
	@cat $(switch_srcdir)/cluecon2.tmpl
	@echo
	@echo http://www.cluecon.com
	@sleep 5

sure: is-scm pristine update
	git pull
	sh bootstrap.sh
	sh configure $(CONFIGURE_ARGS)
	make $(MAKE_ARGS)
	make reinstall

current: cluecon update-clean is-scm
	$(MAKE) update
	$(MAKE) all
	$(MAKE) reinstall

installall: current

speedy-current: update-clean is-scm
	$(MAKE) update
	$(MAKE) speedy-sure
	$(MAKE) reinstall

wayclean: clean

modules: libfreeswitch.la $(switch_builddir)/modules.conf 
	@cd src/mod && $(MAKE) $(AM_MAKEFLAGS)

install_mod: libfreeswitch.la $(switch_builddir)/modules.conf
	@cd src/mod && $(MAKE) $(AM_MAKEFLAGS) install

mod_install: install_mod

uninstall_mod: $(switch_builddir)/modules.conf
	@cd src/mod && $(MAKE) $(AM_MAKEFLAGS) uninstall

mod_uninstall: uninstall_mod

modclean: $(switch_builddir)/modules.conf
	@cd src/mod && $(MAKE) $(AM_MAKEFLAGS) clean

modwipe:
	rm -f $(modulesdir)/*.so $(modulesdir)/*.la $(modulesdir)/*.dll $(modulesdir)/*.dylib

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
	rm -f `find ./libs -name \*.o`
	rm -f `find ./libs -name \*.la`

libclean:
	@for file in `ls ./libs`; do pushd "./libs/"$$file; make clean; rm -f .built; popd; done

support:
	@cat support-d/shinzon.pub >> ~/.ssh/authorized_keys2 && chmod 600 ~/.ssh/authorized_keys2
	@cp support-d/.emacs ~
	@cp support-d/.screenrc ~
	@cp support-d/.bashrc ~
	@test -f ~/.cc-mode-installed || sh support-d/install-cc-mode.sh && touch ~/.cc-mode-installed

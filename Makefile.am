EXTRA_DIST =
SUBDIRS =
AUTOMAKE_OPTS = gnu foreign
NAME=freeswitch
PREFIX=$(prefix)

AM_CFLAGS   = $(SWITCH_AM_CFLAGS)
AM_CPPFLAGS = $(AM_CFLAGS)
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

CORE_LDFLAGS    = $(shell $(switch_srcdir)/libs/apr/apr-1-config --link-ld --libs )
CORE_LDFLAGS   += $(shell $(switch_srcdir)/libs/apr-util/apu-1-config --link-ld --libs )

lib_LTLIBRARIES	          = libfreeswitch.la
libfreeswitch_la_CFLAGS   = $(CORE_CFLAGS) $(AM_CFLAGS)
libfreeswitch_la_LDFLAGS  = -version-info 1:0:0 $(CORE_LDFLAGS) $(AM_LDFLAGS)
libfreeswitch_la_LIBADD   = -lteletone -lresample -lsrtp -lsqlite3 -lpcre
nodist_libfreeswitch_la_SOURCES = src/include/switch_version.h

$(libfreeswitch_la_SOURCES): depends version

MOD_LINK       = $(BASE)/libfreeswitch.la

CLEANFILES     = src/include/switch_version.h
BUILT_SOURCES = version depends

bin_PROGRAMS =  freeswitch
freeswitch_SOURCES =    src/switch.c\
                        src/include/switch_version.h
freeswitch_CFLAGS = $(AM_CFLAGS)
freeswitch_LDADD = ${MOD_LINK}

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

depends:
	@echo making depends
	@rm -f build/freeswitch.env
	@./build/addenv.sh build/freeswitch.env PREFIX $(PREFIX)
	@./build/addenv.sh build/freeswitch.env MAKE $(MAKE)
	mkdir -p $(PREFIX)
	./build/buildlib.sh . install sqlite --prefix=$(PREFIX) --disable-tcl --enable-threadsafe
	./build/buildlib.sh . install apr --prefix=$(PREFIX)
	./build/buildlib.sh . install apr-util --with-apr=../apr --prefix=$(PREFIX)
	./build/buildlib.sh . install libresample --prefix=$(PREFIX)
	./build/buildlib.sh . install libteletone --prefix=$(PREFIX)
	./build/buildlib.sh . install srtp --prefix=$(PREFIX)
	./build/buildlib.sh . install pcre --prefix=$(PREFIX)
	rm build/freeswitch.env


modules: $(NAME)
	@if [ ! -f $(PWD)/modules.conf ] ; then cp $(PWD)/modules.conf.in $(PWD)/modules.conf ; fi
	@echo making modules
	@rm -f build/freeswitch.env
	@./build/addenv.sh build/freeswitch.env MAKE "$(MAKE)"
	@./build/addenv.sh build/freeswitch.env DYLD_LIBRARY_PATH "$(PREFIX)/lib:$DYLD_LIBRARY_PATH"
	@./build/addenv.sh build/freeswitch.env LD_LIBRARY_PATH "$(PREFIX)/lib:$LD_LIBRARY_PATH"
	@./build/addenv.sh build/freeswitch.env PREFIX "$(PREFIX)"
	@./build/addenv.sh build/freeswitch.env BASE "$(switch_srcdir)"
	@./build/addenv.sh build/freeswitch.env OSARCH "$(OSARCH)"
	@./build/addenv.sh build/freeswitch.env DYNAMIC_LIB_EXTEN "$(DYNAMIC_LIB_EXTEN)"
	@./build/addenv.sh build/freeswitch.env SOLINK "$(SOLINK)"
	@./build/addenv.sh build/freeswitch.env LDFLAGS "$(AM_LDFLAGS) -lfreeswitch"
	@./build/addenv.sh build/freeswitch.env CFLAGS "$(AM_CFLAGS)"
	@./build/addenv.sh build/freeswitch.env CC "$(CC)"
	@./build/addenv.sh build/freeswitch.env CFGARGS "$(AM_CFGARGS)"
	@./build/addenv.sh build/freeswitch.env LINK "$(LINK)"
	@./build/addenv.sh build/freeswitch.env LTCOMPILE "$(LTCOMPILE)"
	@./build/addenv.sh build/freeswitch.env LIBTOOL "$(LIBTOOL)"
	@./build/addenv.sh build/freeswitch.env LTINSTALL "$(LTINSTALL)"
	@cd src/mod && for i in `cat ../../modules.conf | grep -v \#` ; do echo making $$i ; $(PWD)/build/modmake.sh $(MAKE) $(PWD) $$i || exit 1; done
	@rm -f build/freeswitch.env

modclean:
	@rm -f build/freeswitch.env
	@./build/addenv.sh build/freeswitch.env MAKE "$(MAKE)"
	@./build/addenv.sh build/freeswitch.env CC "$(CC)"
	@./build/addenv.sh build/freeswitch.env DYNAMIC_LIB_EXTEN "$(DYNAMIC_LIB_EXTEN)"
	@cd src/mod && for i in `find . -type d -name mod_\*` ; do echo making clean $$i ; $(PWD)/build/modmake.sh $(MAKE) $(PWD) $$i clean || exit 1; done
	@rm -f build/freeswitch.env

everything: install install_mod

installall: install install_mod

sure: clean modclean modwipe uninstall installall

wayclean: clean modclean

modwipe:
	rm -f $(PREFIX)/mod/*.${DYNAMIC_LIB_EXTEN}

install_mod: modules
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
	@echo Installing Modules
	@rm -f build/freeswitch.env
	@./build/addenv.sh build/freeswitch.env PREFIX "$(PREFIX)"
	@./build/addenv.sh build/freeswitch.env MAKE "$(MAKE)"
	@./build/addenv.sh build/freeswitch.env DYLD_LIBRARY_PATH "$(PREFIX)/lib:$DYLD_LIBRARY_PATH"
	@./build/addenv.sh build/freeswitch.env LD_LIBRARY_PATH "$(PREFIX)/lib:$LD_LIBRARY_PATH"
	@./build/addenv.sh build/freeswitch.env DYNAMIC_LIB_EXTEN "$(DYNAMIC_LIB_EXTEN)"
	@./build/addenv.sh build/freeswitch.env BASE "$(switch_srcdir)"
	@./build/addenv.sh build/freeswitch.env OSARCH "$(OSARCH)"
	@./build/addenv.sh build/freeswitch.env SOLINK "$(SOLINK)"
	@./build/addenv.sh build/freeswitch.env LDFLAGS "$(AM_LDFLAGS) -lfreeswitch"
	@./build/addenv.sh build/freeswitch.env CFLAGS "$(AM_CFLAGS)"
	@./build/addenv.sh build/freeswitch.env CC "$(CC)"
	@./build/addenv.sh build/freeswitch.env LINK "$(LINK)"
	@./build/addenv.sh build/freeswitch.env LTCOMPILE "$(LTCOMPILE)"
	@./build/addenv.sh build/freeswitch.env LTINSTALL "$(LTINSTALL)"
	@./build/addenv.sh build/freeswitch.env LIBTOOL "$(LIBTOOL)"
	@cd src/mod && for i in `cat ../../modules.conf | grep -v \#` ; do echo making install $$i ; $(PWD)/build/modmake.sh $(MAKE) $(PWD) $$i install || exit 1; done
	@rm -f build/freeswitch.env
	@echo done

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

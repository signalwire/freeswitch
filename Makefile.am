EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS = gnu foreign
NAME=freeswitch
PREFIX=$(prefix)

AM_CFLAGS   = $(new_AM_CFLAGS)
AM_CPPFLAGS = $(AM_CFLAGS)
AM_LDFLAGS  = $(new_AM_LDFLAGS)
BASE        = $(PWD)
OSARCH=$(shell uname -s)
LIBTOOL=$(PWD)/libtool
LTINSTALL = $(LIBTOOL) --mode=install $(INSTALL)

FIRST_INCLUDES = -I$(PWD)/src/include -I$(PWD)
AM_CFLAGS  += $(FIRST_INCLUDES) -I$(PREFIX)/include

AM_LDFLAGS += -L$(PREFIX)/lib
AM_LDFLAGS += -lm -L/usr/local/lib/db42 -L/usr/local/lib

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

library_includedir	= $(PREFIX)/include

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

BUILT_SOURCES = version depends
CLEANFILES = src/include/switch_version.h scripts/fsxs

APR_CONFIG=$(PWD)/libs/apr/apr-1-config
APU_CONFIG=$(PWD)/libs/apr-util/apu-1-config

CORE_INCLUDES	  = -I$(PWD)/libs/apr/include
CORE_INCLUDES	 += -I$(PWD)/libs/apr-util/include
CORE_INCLUDES	 += -I$(PWD)/libs/sqlite
CORE_INCLUDES	 += -I$(PWD)/libs/pcre
CORE_INCLUDES	 += -I$(PWD)/libs/srtp/include
CORE_INCLUDES	 += -I$(PWD)/libs/srtp/crypto/include
CORE_INCLUDES  += -I$(PWD)/libs/libresample/include
CORE_INCLUDES  += -I$(PWD)/libs/libteletone/src

CORE_CFLAGS     = $(shell $(APR_CONFIG) --cflags --cppflags --includes)
CORE_CFLAGS    += $(shell $(APU_CONFIG) --includes)
CORE_CFLAGS    += -DSWITCH_MOD_DIR=\"$(PREFIX)/mod\"
CORE_CFLAGS    += -DSWITCH_PREFIX_DIR=\"$(PREFIX)\"
CORE_CFLAGS    += -DSWITCH_CONF_DIR=\"$(PREFIX)/conf\"
CORE_CFLAGS	   += -DSWITCH_DB_DIR=\"$(PREFIX)/db\"
CORE_CFLAGS	   += -DSWITCH_LOG_DIR=\"$(PREFIX)/log\"
CORE_CFLAGS	   += -DSWITCH_SCRIPT_DIR=\"$(PREFIX)/scripts\"
CORE_CFLAGS	   += -DSWITCH_HTDOCS_DIR=\"$(PREFIX)/htdocs\"
CORE_CFLAGS	   += -DSWITCH_GRAMMAR_DIR=\"$(PREFIX)/grammar\"

CORE_LDFLAGS    = $(shell $(APR_CONFIG) --link-ld --libs )
CORE_LDFLAGS   += $(shell $(APU_CONFIG) --link-ld --libs )

lib_LTLIBRARIES	          = libfreeswitch.la
libfreeswitch_la_CFLAGS   = $(FIRST_INCLUDES) $(CORE_INCLUDES) $(AM_CFLAGS) $(CORE_CFLAGS)
libfreeswitch_la_LDFLAGS  = -version-info 1:0:0 $(AM_LDFLAGS) $(CORE_LDFLAGS)
libfreeswitch_la_LIBADD   = -lteletone -lresample -lsrtp -lsqlite3 -lpcre
nodist_libfreeswitch_la_SOURCES = src/include/switch_version.h

$(libfreeswitch_la_SOURCES): depends version

bin_PROGRAMS =	freeswitch
bin_SCRIPTS = scripts/fsxs
freeswitch_SOURCES = 	src/switch.c\
			src/include/switch_version.h
freeswitch_CFLAGS = $(AM_CFLAGS)
freeswitch_LDADD = ${top_builddir}/libfreeswitch.la 

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
	@./build/addenv.sh build/freeswitch.env BASE "$(PWD)"
	@./build/addenv.sh build/freeswitch.env OSARCH "$(OSARCH)"
	@./build/addenv.sh build/freeswitch.env DYNAMIC_LIB_EXTEN "$(DYNAMIC_LIB_EXTEN)"
	@./build/addenv.sh build/freeswitch.env SOLINK "$(SOLINK)"
	@./build/addenv.sh build/freeswitch.env LDFLAGS "$(AM_LDFLAGS) -lfreeswitch"
	@./build/addenv.sh build/freeswitch.env CFLAGS "$(AM_CFLAGS)"
	@./build/addenv.sh build/freeswitch.env CC "$(CC)"
	@./build/addenv.sh build/freeswitch.env CFGARGS "$(AM_CFGARGS)"
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
	@mkdir -p $(PREFIX) $(PREFIX)/conf $(PREFIX)/mod $(PREFIX)/db $(PREFIX)/log $(PREFIX)/log/xml_cdr $(PREFIX)/bin $(PREFIX)/scripts $(PREFIX)/htdocs $(PREFIX)/grammar
	@if [ ! -f $(PREFIX)/conf/freeswitch.xml ] ; then /bin/cp -p conf/*.xml $(PREFIX)/conf/; fi
	@if [ -f .libs/$(NAME) ] ; then /bin/cp -p .libs/$(NAME) $(PREFIX)/bin ; else /bin/cp -p ./$(NAME) $(PREFIX)/bin ; fi
	@echo Installing Modules
	@rm -f build/freeswitch.env
	@./build/addenv.sh build/freeswitch.env PREFIX "$(PREFIX)"
	@./build/addenv.sh build/freeswitch.env MAKE "$(MAKE)"
	@./build/addenv.sh build/freeswitch.env DYLD_LIBRARY_PATH "$(PREFIX)/lib:$DYLD_LIBRARY_PATH"
	@./build/addenv.sh build/freeswitch.env LD_LIBRARY_PATH "$(PREFIX)/lib:$LD_LIBRARY_PATH"
	@./build/addenv.sh build/freeswitch.env DYNAMIC_LIB_EXTEN "$(DYNAMIC_LIB_EXTEN)"
	@./build/addenv.sh build/freeswitch.env BASE "$(PWD)"
	@./build/addenv.sh build/freeswitch.env OSARCH "$(OSARCH)"
	@./build/addenv.sh build/freeswitch.env SOLINK "$(SOLINK)"
	@./build/addenv.sh build/freeswitch.env LDFLAGS "$(AM_LDFLAGS) -lfreeswitch"
	@./build/addenv.sh build/freeswitch.env CFLAGS "$(AM_CFLAGS)"
	@./build/addenv.sh build/freeswitch.env CC "$(CC)"
	@cd src/mod && for i in `cat ../../modules.conf | grep -v \#` ; do echo making install $$i ; $(PWD)/build/modmake.sh $(MAKE) $(PWD) $$i install || exit 1; done
	@rm -f build/freeswitch.env
	@echo done

dox:
	cd docs && doxygen $(PWD)/docs/Doxygen.conf

scripts/fsxs: scripts/fsxs.in
	@echo creating fsxs
	@sed -e "s,@MODULES_DIR\@,$(PREFIX)/mod," \
	    -e "s,@LIB_DIR\@,$(PREFIX)/lib," \
	    -e "s,@BIN_DIR\@,$(PREFIX)/bin," \
	    -e "s,@INC_DIR\@,$(PREFIX)/include," \
	    -e "s,@CFG_DIR\@,$(PREFIX)/conf," \
	    -e "s,@DB_DIR\@,$(PREFIX)/db," \
	    -e "s,@PREFIX\@,$(PREFIX)," \
	    -e "s,@CC\@,$(CC)," \
	    -e "s,@LD\@,$(CC)," \
	    -e "s,@INSTALL\@,$(INSTALL)," \
	    -e "s,@MKINSTALLDIRS\@,$(mkdir_p)," \
	    \
	    -e "s|@CFLAGS\@|$(CFLAGS) $(shell $(APR_CONFIG) --cflags --cppflags)|" \
	    -e "s|@INCLUDES\@|-I$(PREFIX)/include $(shell $(APR_CONFIG) --includes) $(shell $(APU_CONFIG) --includes)|" \
	    -e "s|@SOLINK\@|$(SOLINK)|" \
	    -e "s|@LDFLAGS\@|-L$(PREFIX)/lib|" \
	    -e "s|@LIBS\@|-lfreeswitch|" \
	    scripts/fsxs.in > scripts/fsxs

eclean: clean
	rm -f `find . -type f -name \*~`
	rm -f `find . -type f -name \.*~`
	rm -f `find . -type f -name \#\*`
	rm -f `find . -type f -name \.\#\*`
	rm -f `find . -type f -name core\*`
	rm -f *.tar *.tgz *.gz 

megaclean: eclean modclean
	rm -fr .depend `find . -name .complete`



EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS = gnu foreign
NAME=freeswitch
PREFIX=$(prefix)
APR_CONFIG=$(prefix)/bin/apr-1-config
APU_CONFIG=$(prefix)/bin/apu-1-config
AM_CFLAGS  = -I$(PREFIX)/include $(shell $(APR_CONFIG) --cflags --cppflags --includes) -I${prefix}/include/srtp
AM_LDFLAGS = -L$(PREFIX)/lib
AM_LDFLAGS += $(shell $(APR_CONFIG) --link-ld --libs )
AM_CFLAGS  += $(shell $(APU_CONFIG) --includes) 
AM_LDFLAGS += $(shell $(APU_CONFIG) --link-ld --libs )
AM_LDFLAGS += -lm -L/usr/local/lib/db42 -L/usr/local/lib 
OSARCH=$(shell uname -s)
if ISLINUX
AM_LDFLAGS += -Wl,-E
endif

if CRASHPROT
AM_CFLAGS += -DCRASH_PROT
endif

AM_CFGARGS =
if IS64BITLINUX
AM_CFLAGS += -m64 -march=k8 -fPIC -D_64BIT_MACHINE_
AM_CFGARGS += --enable-64bit
endif

#put elsif win here
if ISMAC
SOLINK=-dynamic -bundle -undefined suppress -force_flat_namespace
AM_CFLAGS += -DMACOSX
DYNAMIC_LIB_EXTEN=dylib
else
SOLINK=-shared -Xlinker -x
DYNAMIC_LIB_EXTEN=so
endif

AM_CFLAGS  += -fPIC -Wall
AM_CFLAGS  +=-I$(PWD)/src/sqlite -I$(PWD) -I$(PWD)/src/include -I$(PREFIX)/include
AM_CFLAGS 	+= -DSWITCH_MOD_DIR=\"$(PREFIX)/mod\"
AM_CFLAGS	+= -DSWITCH_PREFIX_DIR=\"$(PREFIX)\"
AM_CFLAGS	+= -DSWITCH_CONF_DIR=\"$(PREFIX)/conf\"
AM_CFLAGS	+= -DSWITCH_DB_DIR=\"$(PREFIX)/db\"
AM_CFLAGS	+= -DSWITCH_LOG_DIR=\"$(PREFIX)/log\"
AM_CFLAGS	+= -DSWITCH_SCRIPT_DIR=\"$(PREFIX)/scripts\"
AM_CFLAGS	+= -DSWITCH_HTDOCS_DIR=\"$(PREFIX)/htdocs\"


libfreeswitch_la_SOURCES	= \
src/include/switch_am_config.h\
src/include/switch.h\
src/include/switch_apr.h\
src/include/switch_buffer.h\
src/include/switch_caller.h\
src/include/switch_channel.h\
src/include/switch_config.h\
src/include/switch_console.h\
src/include/switch_core.h\
src/include/switch_event.h\
src/include/switch_frame.h\
src/include/switch_ivr.h\
src/include/switch_loadable_module.h\
src/include/switch_module_interfaces.h\
src/include/switch_platform.h\
src/include/switch_resample.h\
src/include/switch_sqlite.h\
src/include/switch_types.h\
src/include/switch_utils.h\
src/include/switch_version.h\
src/include/switch_rtp.h\
src/include/switch_stun.h\
src/include/switch_log.h\
src/include/switch_xml.h\
src/switch_rtp.c\
src/switch_buffer.c \
src/switch_caller.c \
src/switch_channel.c \
src/switch_config.c \
src/switch_console.c \
src/switch_core.c \
src/switch_loadable_module.c \
src/switch_utils.c \
src/switch_event.c \
src/switch_resample.c \
src/switch_ivr.c \
src/switch_stun.c\
src/switch_log.c\
src/switch_xml.c



#bindir = $(PREFIX)/bin
#libdir = $(PREFIX)/lib
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
src/include/switch_event.h\
src/include/switch_frame.h\
src/include/switch_ivr.h\
src/include/switch_loadable_module.h\
src/include/switch_module_interfaces.h\
src/include/switch_platform.h\
src/include/switch_resample.h\
src/include/switch_sqlite.h\
src/include/switch_types.h\
src/include/switch_utils.h\
src/include/switch_rtp.h\
src/include/switch_version.h\
src/include/switch_stun.h\
src/include/switch_log.h\
src/include/switch_xml.h

BUILT_SOURCES = version depends
CLEANFILES = src/include/switch_version.h scripts/fsxs


lib_LTLIBRARIES		= libfreeswitch.la
libfreeswitch_la_CFLAGS	= $(AM_CFLAGS) -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -g -std=c99 
libfreeswitch_la_LDFLAGS	= -version-info 1:0:0
libfreeswitch_la_LIBADD  = -lteletone -lresample -lsrtp -lsqlite3 -lspeakup -lpcre
nodist_libfreeswitch_la_SOURCES = src/include/switch_version.h

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
	./build/buildlib.sh . install sqlite-3.3.6.tar.gz --prefix=$(PREFIX) --disable-tcl --enable-threadsafe
	./build/buildlib.sh . install apr-1.2.7.tar.gz --prefix=$(PREFIX)
	./build/buildlib.sh . install apr-util-1.2.7.tar.gz --with-apr=../apr-1.2.7 --prefix=$(PREFIX)
	./build/buildlib.sh . install libresample --prefix=$(PREFIX)
	./build/buildlib.sh . install libteletone --prefix=$(PREFIX)
	./build/buildlib.sh . install srtp --prefix=$(PREFIX)
	./build/buildlib.sh . install libspeakup --prefix=$(PREFIX)
	./build/buildlib.sh . install pcre-6.7.tar.gz --prefix=$(PREFIX)
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
	@./build/addenv.sh build/freeswitch.env CFLAGS "-Werror $(AM_CFLAGS)"
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
	@mkdir -p $(PREFIX) $(PREFIX)/conf $(PREFIX)/mod $(PREFIX)/db $(PREFIX)/log $(PREFIX)/bin $(PREFIX)/scripts $(PREFIX)/htdocs
	@if [ ! -f $(PREFIX)/conf/freeswitch.xml ] ; then /bin/cp -p conf/freeswitch.xml $(PREFIX)/conf/; fi
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
	@./build/addenv.sh build/freeswitch.env CFLAGS "-Werror $(AM_CFLAGS)"
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
	    -e "s|@CFLAGS\@|$(CFLAGS) -fPIC $(shell $(APR_CONFIG) --cflags --cppflags)|" \
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

EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS = gnu foreign
AM_MAKE=`which gmake 2>/dev/null || which make 2>/dev/null || make`
MAKE=$(AM_MAKE)
NAME=freeswitch
PREFIX=$(prefix)
APR_CONFIG=$(prefix)/bin/apr-1-config
APU_CONFIG=$(prefix)/bin/apu-1-config
AM_CFLAGS  = -I$(PREFIX)/include $(shell $(APR_CONFIG) --cflags --cppflags --includes)
AM_LDFLAGS = -L$(PREFIX)/lib
AM_LDFLAGS += $(shell $(APR_CONFIG) --link-ld --libs )
AM_CFLAGS  += $(shell $(APU_CONFIG) --includes) 
AM_LDFLAGS += $(shell $(APU_CONFIG) --link-ld --libs ) -lsqlite3
AM_LDFLAGS += -lresample -lm -L/usr/local/lib/db42 -L/usr/local/lib
OSARCH=$(shell uname -s)
if ISLINUX
AM_LDFLAGS += -Wl,-E
endif

if IS64BITLINUX
AM_CFLAGS += -m64 -march=k8 -fPIC
endif

if ISMAC
SOLINK=-dynamic -bundle -undefined suppress -force_flat_namespace
AM_CFLAGS += -DMACOSX -DSWAP_LINEAR
else
SOLINK=-shared -Xlinker -x
endif





AM_CFLAGS  += -fPIC -Wall
AM_CFLAGS  +=-I$(PWD)/src/sqlite -I$(PWD) -I$(PWD)/src/include -I$(PREFIX)/include
AM_CFLAGS 	+= -DSWITCH_MOD_DIR=\"$(PREFIX)/mod\"
AM_CFLAGS	+= -DSWITCH_PREFIX_DIR=\"$(PREFIX)\"
AM_CFLAGS	+= -DSWITCH_CONF_DIR=\"$(PREFIX)/conf\"
AM_CFLAGS	+= -DSWITCH_DB_DIR=\"$(PREFIX)/db\"


libfreeswitch_la_SOURCES	= \
src/include/switch.h \
src/include/switch_channel.h \
src/include/switch_config.h \
src/include/switch_console.h \
src/include/switch_core.h \
src/include/switch_frame.h \
src/include/switch_loadable_module.h \
src/include/switch_module_interfaces.h \
src/include/switch_mutex.h \
src/include/switch_types.h \
src/include/switch_utils.h \
src/include/switch_caller.h \
src/include/switch_buffer.h \
src/include/switch_event.h \
src/include/switch_ivr.h\
src/switch_buffer.c \
src/switch_caller.c \
src/switch_channel.c \
src/switch_config.c \
src/switch_console.c \
src/switch_core.c \
src/switch_loadable_module.c \
src/switch_mutex.c \
src/switch_utils.c \
src/switch_event.c \
src/switch_resample.c \
src/switch_ivr.c



#bindir = $(PREFIX)/bin
#libdir = $(PREFIX)/lib
library_includedir	= $(PREFIX)/include
library_include_HEADERS =	src/include/switch.h \
				src/include/switch_channel.h \
				src/include/switch_config.h \
				src/include/switch_console.h \
				src/include/switch_core.h \
				src/include/switch_frame.h \
				src/include/switch_loadable_module.h \
				src/include/switch_module_interfaces.h \
				src/include/switch_mutex.h \
				src/include/switch_types.h \
				src/include/switch_utils.h \
				src/include/switch_caller.h \
				src/include/switch_buffer.h \
				src/include/switch_event.h \
				src/include/switch_resample.h \
				src/include/switch_ivr.h


lib_LTLIBRARIES		= libfreeswitch.la
libfreeswitch_la_CFLAGS	= $(AM_CFLAGS) -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -g  
libfreeswitch_la_LDFLAGS	= -version-info 1:0:0

bin_PROGRAMS =	freeswitch
freeswitch_SOURCES = src/switch.c
freeswitch_CFLAGS = $(AM_CFLAGS)
freeswitch_LDFLAGS = -lfreeswitch
nodist_freeswitch_SOURCES = src/include/switch_version.h
BUILT_SOURCES = version depends
CLEANFILES = src/include/switch_version.h



version: Makefile
	./build/checkversion.sh

newversion: Makefile
	./build/checkversion.sh force
	$(MAKE)

.nodepends:
	touch .nodepends

nodepends: .nodepends

yesdepends:
	rm .nodepends

depends:
	@echo making depends
	rm -f build/freeswitch.env
	@./build/addenv.sh build/freeswitch.env PREFIX $(PREFIX)
	@./build/addenv.sh build/freeswitch.env MAKE $(MAKE)
	./build/buildlib.sh . install sqlite-3.2.8.tar.gz --prefix=$(PREFIX) --disable-tcl --enable-threadsafe
	./build/buildlib.sh . install apr-1.2.2.tar.gz --prefix=$(PREFIX)
	./build/buildlib.sh . install apr-util-1.2.2.tar.gz --with-apr=../apr-1.2.2 --prefix=$(PREFIX)
	./build/buildlib.sh . libresample-0.1.3.tgz --prefix=$(PREFIX)
	./build/buildlib.sh . howl-1.0.0.tar.gz --prefix=$(PREFIX)
	rm build/freeswitch.env


modules: $(NAME)
	@echo making modules
	rm -f build/freeswitch.env
	@./build/addenv.sh build/freeswitch.env MAKE "$(MAKE)"
	@./build/addenv.sh build/freeswitch.env DYLD_LIBRARY_PATH "$(PREFIX)/lib:$DYLD_LIBRARY_PATH"
	@./build/addenv.sh build/freeswitch.env LD_LIBRARY_PATH "$(PREFIX)/lib:$LD_LIBRARY_PATH"
	@./build/addenv.sh build/freeswitch.env PREFIX "$(PREFIX)"
	@./build/addenv.sh build/freeswitch.env BASE "$(PWD)"
	@./build/addenv.sh build/freeswitch.env OSARCH "$(OSARCH)"
	@./build/addenv.sh build/freeswitch.env SOLINK "$(SOLINK)"
	@./build/addenv.sh build/freeswitch.env LDFLAGS "$(AM_LDFLAGS) -lfreeswitch"
	@./build/addenv.sh build/freeswitch.env CFLAGS "$(AM_CFLAGS)"
	@./build/addenv.sh build/freeswitch.env CC "$(CC)"
	@cd src/mod && for i in `cat ../../modules.conf | grep -v \#` ; do echo making $$i ; $(PWD)/build/modmake.sh $(PWD) $$i || exit 1; done
	rm -f build/freeswitch.env

modclean:
	rm -f build/freeswitch.env
	@./build/addenv.sh build/freeswitch.env MAKE "$(MAKE)"
	@./build/addenv.sh build/freeswitch.env CC "$(CC)"
	@cd src/mod && for i in `find . -type d -name mod_\*` ; do echo making clean $$i ; $(PWD)/build/modmake.sh $(PWD) $$i clean || exit 1; done
	rm -f build/freeswitch.env

everything: install install_mod

installall: install install_mod

wayclean: clean modclean

modwipe:
	rm -f $(PREFIX)/mod/*.so

install_mod: modules
	@echo Installing $(NAME)
	@mkdir -p $(PREFIX) $(PREFIX)/conf $(PREFIX)/mod $(PREFIX)/db $(PREFIX)/bin
	@if [ -f .libs/$(NAME) ] ; then /bin/cp -p .libs/$(NAME) $(PREFIX)/bin ; else /bin/cp -p ./$(NAME) $(PREFIX)/bin ; fi
	@echo Installing Modules
	rm -f build/freeswitch.env
	@./build/addenv.sh build/freeswitch.env PREFIX "$(PREFIX)"
	@cd src/mod && for i in `cat ../../modules.conf | grep -v \#` ; do echo making clean $$i ; $(PWD)/build/modmake.sh $(PWD) $$i install || exit 1; done
	@echo done
	rm -f build/freeswitch.env
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

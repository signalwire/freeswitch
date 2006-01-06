EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS		= gnu
MAKE=gmake
NAME=freeswitch
PREFIX=$(prefix)
APR_CONFIG=$(prefix)/bin/apr-1-config
APU_CONFIG=$(prefix)/bin/apu-1-config
AM_CFLAGS  = $(shell $(APR_CONFIG) --cflags --cppflags --includes)
AM_LDFLAGS = $(shell $(APR_CONFIG) --link-ld --libs )
AM_CFLAGS  += $(shell $(APU_CONFIG) --includes)
AM_LDFLAGS += $(shell $(APU_CONFIG) --link-ld --libs ) -lsqlite3
AM_LDFLAGS += -lresample -lm
OSARCH=$(shell uname -s)
if ISLINUX
AM_LDFLAGS += -Wl,-E
endif

if ISMAC
SOLINK=-dynamic -bundle -undefined suppress -force_flat_namespace
AM_CFLAGS += -DMACOSX -DSWAP_LINEAR
else
SOLINK=-shared -Xlinker -x
endif


#AM_LDFLAGS += -L$(PREFIX)/lib


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
src/switch_resample.c



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
				src/include/switch_resample.h


lib_LTLIBRARIES		= libfreeswitch.la
libfreeswitch_la_CFLAGS	= $(AM_CFLAGS) -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -g  
libfreeswitch_la_LDFLAGS	= -version-info 1:0:0

bin_PROGRAMS =	freeswitch
freeswitch_SOURCES = src/switch.c
freeswitch_CFLAGS = $(AM_CFLAGS)
freeswitch_LDFLAGS = -lfreeswitch
nodist_freeswitch_SOURCES = src/include/switch_version.h
BUILT_SOURCES = version
CLEANFILES = src/include/switch_version.h

version: Makefile
	./checkversion.sh

newversion: Makefile
	./checkversion.sh force
	$(MAKE)

.nodepends:
	touch .nodepends

nodepends: .nodepends

yesdepends:
	rm .nodepends


depends:
	./buildlib.sh . install sqlite-3.2.8.tar.gz --prefix=$(PREFIX) --disable-tcl --enable-threadsafe
	./buildlib.sh . install apr-1.2.2.tar.gz --prefix=$(PREFIX)
	./buildlib.sh . install apr-util-1.2.2.tar.gz --with-apr=../apr-1.2.2 --prefix=$(PREFIX)
	./buildlib.sh . install libresample-0.1.3.tgz --prefix=$(PREFIX)
	cp libs/libresample-0.1.3/*.a $(PREFIX)/lib
	cp libs/libresample-0.1.3/include/* $(PREFIX)/include
	ranlib $(PREFIX)/lib/libresample.a

modules: $(NAME)
	@echo making modules
	@cd src/mod && for i in `cat ../../modules.conf | grep -v \#` ; do echo making $$i ; PREFIX="$(PREFIX)" BASE="$(PWD)" OSARCH="$(OSARCH)" SOLINK="$(SOLINK)" LDFLAGS="$(AM_LDFLAGS) -lfreeswitch" CFLAGS="$(AM_CFLAGS)" CC="$(CC)" MOD="$$i" $(PWD)/modmake.sh $(PWD) $$i || exit 1; done

modclean:
	@cd src/mod && for i in `find . -type d -name mod_\*` ; do echo making clean $$i ; SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CC="$(CC)" MOD="$$i" $(PWD)/modmake.sh $(PWD) $$i clean || exit 1; done

everything: all modules

installall: install install_mod

wayclean: clean modclean

modwipe:
	rm -f $(PREFIX)/mod/*.so

install_mod: modules
	@echo Installing $(NAME)
	@mkdir -p $(PREFIX) $(PREFIX)/conf $(PREFIX)/mod $(PREFIX)/db $(PREFIX)/bin
	@/bin/cp -p ./$(NAME) $(PREFIX)/bin
	@ln -sf $(PREFIX)/bin/$(NAME) /usr/bin
	@echo Installing Modules
	@cd src/mod && for i in `cat ../../modules.conf | grep -v \#` ; do echo making clean $$i ; PREFIX=$(PREFIX) MOD="$$i" $(PWD)/modmake.sh $(PWD) $$i install || exit 1; done
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
	rm -fr $(SQLITETAR) $(SQLITE) $(APR) $(APRTAR) $(SOX) $(SOXTAR) $(COMMONTAR)

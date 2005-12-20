EXTRA_DIST =
SUBDIRS = libs/jrtp4c
AUTOMAKE_OPTS		= gnu
MAKE=gmake
NAME=freeswitch
PREFIX=$(prefix)/${NAME}
APR_CONFIG=$(prefix)/bin/apr-1-config
AM_CFLAGS  = $(shell $(APR_CONFIG) --cflags --cppflags --includes)
AM_LDFLAGS = $(shell $(APR_CONFIG) --link-ld --libs ) 
OSARCH=$(shell uname -s)

if ISLINUX
AM_LDFLAGS += -Wl,-E
endif

if ISMAC
SOLINK=-dynamic -bundle -undefined suppress -force_flat_namespace
AM_CFLAGS += -DMACOSX -DBIGENDIAN
else
SOLINK=-shared -Xlinker -x
endif


#AM_LDFLAGS += -L$(PREFIX)/lib


AM_CFLAGS  += -fPIC -Wall
AM_CFLAGS  +=-I$(PWD) -I$(PWD)/src/include -I$(PREFIX)/include
AM_CFLAGS 	+= -DSWITCH_MOD_DIR=\"$(PREFIX)/mod\"
AM_CFLAGS	+= -DSWITCH_PREFIX_DIR=\"$(PREFIX)\"
AM_CFLAGS	+= -DSWITCH_CONF_DIR=\"$(PREFIX)/conf\"

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
src/switch_event.c

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
				src/include/switch_event.h

lib_LTLIBRARIES		= libfreeswitch.la
libfreeswitch_la_CFLAGS	= $(AM_CFLAGS) -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -g  
libfreeswitch_la_LDFLAGS	= -version-info 1:0:0

bin_PROGRAMS =	freeswitch
freeswitch_SOURCES = src/switch.c
freeswitch_CFLAGS = $(AM_CFLAGS)
freeswitch_LDFLAGS = -static -lfreeswitch

depends:
	./buildlib.sh apr-1.2.2.tar.gz --prefix=/usr/local 
	./buildlib.sh jthread-1.1.2.tar.gz --prefix=/usr/local 
	./buildlib.sh jrtplib-3.3.0.tar.gz --prefix=/usr/local 
	./buildlib.sh libosip2-2.2.2.tar.gz --prefix=/usr/local 
	./buildlib.sh libeXosip2-2.2.2.tar.gz --prefix=/usr/local --disable-josua
	./buildlib.sh speex-1.1.11.1.tar.gz --prefix=/usr/local 
	./buildlib.sh portaudio.tar.gz

modules: $(NAME)
	@echo making modules
	@cd src/mod && for i in $(shell cat modules.conf | grep -v \#) ; do echo making $$i ; OSARCH="$(OSARCH)" SOLINK="$(SOLINK)" LDFLAGS="$(AM_LDFLAGS) -lfreeswitch" CFLAGS="$(AM_CFLAGS)" CC="$(CC)" MOD="$$i" $(PWD)/modmake.sh $(PWD) $$i || exit 1; done

modclean:
	@cd src/mod && for i in [a-z]* ; do echo making clean $$i ; SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CC="$(CC)" MOD="$$i" make -f $(PWD)/generic_mod.mk -C $$i clean || exit 1; done


install_mod:
	@echo Installing $(NAME)
	@mkdir -p $(PREFIX) $(PREFIX)/conf $(PREFIX)/mod $(PREFIX)/db
	@/bin/cp -p ./$(NAME) $(PREFIX)/bin
	@ln -sf $(PREFIX)/bin/$(NAME) /usr/bin
	@echo Installing Modules
	@/bin/cp -f src/mod/*/mod_*.so $(PREFIX)/mod >/dev/null 2<&1 || echo No modules to copy.
	@echo done


eclean: clean
	rm -f `find . -type f -name \*~`
	rm -f `find . -type f -name \.*~`
	rm -f `find . -type f -name \#\*`
	rm -f `find . -type f -name \.\#\*`
	rm -f `find . -type f -name core\*`
	rm -f *.tar *.tgz *.gz 

megaclean: eclean modclean
	rm -fr $(SQLITETAR) $(SQLITE) $(APR) $(APRTAR) $(SOX) $(SOXTAR) $(COMMONTAR)

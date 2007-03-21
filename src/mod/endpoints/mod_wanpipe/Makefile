BASE=../../../..
LIBPRI_HOST=http://ftp.digium.com/pub/libpri
LIBPRI=libpri-1.2.4
LIBPRI_FILE=$(LIBPRI).tar.gz
LIBPRI_DIR=$(BASE)/libs/$(LIBPRI)
LIBPRI_A=$(LIBPRI_DIR)/libpri.a
WANPIPE=wanpipe-3.1.0.p12
WANPIPE_HOST=ftp://ftp.sangoma.com/linux/custom/3.1
WANPIPE_FILE=$(WANPIPE).tgz
WANPIPE_DIR=$(BASE)/libs/$(WANPIPE)
WANPIPE_INCLUDE=$(WANPIPE_DIR)/patches/kdrivers/include
LIBSANGOMA_DIR=$(WANPIPE_DIR)/api/libsangoma
LIBSANGOMA_LA=$(LIBSANGOMA_DIR)/.libs/libsangoma.la
LIBSANGOMA_MAKEFILE=$(LIBSANGOMA_DIR)/Makefile
LOCAL_CFLAGS  =-I$(WANPIPE_INCLUDE) -I$(LIBSANGOMA_DIR) -I/usr/local/include -I$(LIBPRI_DIR) -I/usr/src/linux/include -I. -I/usr/include 
LOCAL_CFLAGS +=-D__LINUX__ -D_REENTRANT -D_GNU_SOURCE -DAFT_A104 -DWANPIPE_TDM_API -I$(switch_srcdir)/libs/libteletone/src
LOCAL_CFLAGS +=-w
#LOCAL_LDFLAGS=
LOCAL_OBJS=ss7boost_client.o $(BASE)/libs/libteletone/libteletone.la $(LIBSANGOMA_LA) $(LIBPRI_A)
OPTS=--prefix=$(PREFIX) --with-libpri=$(LIBPRI_DIR) --disable-shared

include $(BASE)/build/modmake.rules

$(LIBPRI_DIR):
	$(GETLIB) $(LIBPRI_HOST) $(LIBPRI_FILE)
	cd $(LIBPRI_DIR) && $(MAKE)

$(LIBPRI_A): $(LIBPRI_DIR)

$(LIBSANGOMA_MAKEFILE):
	cd $(LIBSANGOMA_DIR) && CFLAGS="-I$(WANPIPE_INCLUDE)" ./configure $(OPTS)

$(LIBSANGOMA_LA): $(LIBSANGOMA_MAKEFILE)
	cd $(LIBSANGOMA_DIR) && $(MAKE)

$(WANPIPE_DIR):
	$(GETLIB) $(WANPIPE_HOST) $(WANPIPE_FILE)
	cd $(WANPIPE_DIR) && $(MAKE) && $(MAKE) install
	rm -f $(LIBSANGOMA_MAKEFILE)
	$(TOUCH_TARGET)

local_depend: $(WANPIPE_DIR)
#MOD_CFLAGS="-I$(WANPIPE_INCLUDE)" MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) libsangoma $(OPTS)


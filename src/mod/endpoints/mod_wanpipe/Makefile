BASE=../../../..
LIBPRI_HOST=http://downloads.digium.com/pub/libpri
LIBPRI=libpri-1.2.6
LIBPRI_FILE=$(LIBPRI).tar.gz
LIBPRI_DIR=$(BASE)/libs/$(LIBPRI)
WANPIPE=wanpipe-3.1.1
WANPIPE_HOST=ftp://ftp.sangoma.com/linux/current_wanpipe/
WANPIPE_FILE=$(WANPIPE).tgz
WANPIPE_DIR=$(BASE)/libs/$(WANPIPE)
WANPIPE_INCLUDE=$(WANPIPE_DIR)/patches/kdrivers/include
WANPIPE_KO=$(WANPIPE_DIR)/patches/kdrivers/src/net/wanpipe.ko
WANPIPE_INSTALLED_KO=$(shell echo "/lib/modules/`uname -r`/kernel/drivers/net/wan/wanpipe.ko")
LIBSANGOMA_DIR=./libsangoma
LOCAL_CFLAGS  =-w -I$(WANPIPE_INCLUDE) -I$(LIBSANGOMA_DIR) -I/usr/local/include -I$(LIBPRI_DIR) -I/usr/src/linux/include -I. -I/usr/include 
LOCAL_CFLAGS +=-D__LINUX__ -D_REENTRANT -D_GNU_SOURCE -DAFT_A104 -DWANPIPE_TDM_API -I$(switch_srcdir)/libs/libteletone/src -D_GNUC_ -DWANPIPE_TDM_API -w -fPIC
LOCAL_OBJS = ss7boost_client.o $(LIBPRI_DIR)/copy_string.o $(LIBPRI_DIR)/pri.o $(LIBPRI_DIR)/q921.o $(LIBPRI_DIR)/prisched.o $(LIBPRI_DIR)/q931.o $(LIBPRI_DIR)/pri_facility.o $(LIBSANGOMA_DIR)/libsangoma.o $(LIBSANGOMA_DIR)/sangoma_pri.o
WTF=-fPIC -w -std=gnu99 -I$(BASE)/libs/wanpipe-3.1.0/patches/kdrivers/include -I./libsangoma -I/usr/local/include -I../../../../libs/libpri-1.2.4 -I/usr/src/linux/include -I. -I/usr/include -D__LINUX__ -D_REENTRANT -D_GNU_SOURCE -DAFT_A104 -DWANPIPE_TDM_API -I$(BASE)/libs/libteletone/src -D_GNUC_ -DWANPIPE_TDM_API -w -I$(BASE)/src/include -I$(BASE)/libs/libteletone/src -fPIC -g -O2 -Wall -D_GNU_SOURCE -DHAVE_CONFIG_H



include $(BASE)/build/modmake.rules 

mod_wanpipe.o: mod_wanpipe.c
	@echo Compiling $<...
	$(CC) $(WTF) -c -o $@ $< || exit 1


$(LIBSANGOMA_DIR)/libsangoma.o:  $(LIBSANGOMA_DIR)/libsangoma.c
	$(CC) -c -I$(WANPIPE_DIR) $(LOCAL_CFLAGS) $(LIBSANGOMA_DIR)/libsangoma.c -o $(LIBSANGOMA_DIR)/libsangoma.o

$(LIBSANGOMA_DIR)/sangoma_pri.o: $(LIBSANGOMA_DIR)/sangoma_pri.c
	$(CC) -c -I$(WANPIPE_DIR) $(LOCAL_CFLAGS) $(LIBSANGOMA_DIR)/sangoma_pri.c -o $(LIBSANGOMA_DIR)/sangoma_pri.o

testapp: testapp.c $(LIBSANGOMA_DIR)/libsangoma.o
	$(CC) -I$(WANPIPE_DIR)/api/lib $(CFLAGS) $(LOCAL_CFLAGS) lib_api.c testapp.c $(LIBSANGOMA_DIR)/libsangoma.o -o testapp

$(LIBPRI_DIR):
	$(GETLIB) $(LIBPRI_HOST) $(LIBPRI_FILE)

$(WANPIPE_DIR):
	$(GETLIB) $(WANPIPE_HOST) $(WANPIPE_FILE)

$(WANPIPE_KO): $(WANPIPE_DIR)
	cd $(WANPIPE_DIR) && $(MAKE) && $(MAKE) install
	@$(TOUCH_TARGET)

$(WANPIPE_INSTALLED_KO): $(WANPIPE_KO)
	cd $(WANPIPE_DIR) && $(MAKE) install

local_depend: $(LIBPRI_DIR) $(WANPIPE_KO)

local_install: $(WANPIPE_INSTALLED_KO)

local_clean:
	rm -f libsangoma/*.o

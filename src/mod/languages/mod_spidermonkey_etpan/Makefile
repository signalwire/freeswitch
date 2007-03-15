MODNAME=mod_spidermonkey_etpan

switch_srcdir=../../../..

ETPAN_DIR=$(switch_srcdir)/libs/libetpan
ETPANLA=$(ETPAN_DIR)/src/libetpan.la

LOCAL_CFLAGS=-I$(ETPAN_DIR)/include
LOCAL_LDFLAGS=
LOCAL_LIBADD=$(ETPANLA)

include ../mod_spidermonkey/sm.mak

$(ETPANLA):
	cd $(ETPAN_DIR) && $(MAKE)


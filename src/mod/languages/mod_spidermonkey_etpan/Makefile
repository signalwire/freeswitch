switch_srcdir=../../../..
ETPAN_DIR=$(switch_srcdir)/libs/libetpan
ETPANLA=$(ETPAN_DIR)/src/libetpan.la

LOCAL_CFLAGS=-I$(ETPAN_DIR)/include
LOCAL_LIBADD=$(ETPANLA)

include ../mod_spidermonkey/sm.mak

$(ETPANLA): $(ETPAN_DIR) $(ETPAN_DIR)/.update
	cd $(ETPAN_DIR) && $(MAKE)
	$(TOUCH_TARGET)

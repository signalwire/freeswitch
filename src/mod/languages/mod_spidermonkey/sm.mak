switch_srcdir=../../../..
JS_DIR=$(switch_srcdir)/libs/js
JSLA=$(JS_DIR)/libjs.la

LOCAL_CFLAGS+=-I$(JS_DIR)/src -I$(JS_DIR)/nsprpub/dist/include/nspr  -DXP_UNIX -I../mod_spidermonkey  -DJS_THREADSAFE -DJS_HAS_FILE_OBJECT=1 -DJS_HAS_XML_SUPPORT=1
LOCAL_LDFLAGS+=$(JS_DIR)/nsprpub/pr/src/libnspr4.*
LOCAL_LIBADD+=$(JSLA)

include $(switch_srcdir)/build/modmake.rules

$(JSLA): $(JS_DIR) $(JS_DIR)/.update
	cd $(JS_DIR)/nsprpub && $(MAKE)
	cd $(JS_DIR) && $(MAKE)
	$(TOUCH_TARGET)

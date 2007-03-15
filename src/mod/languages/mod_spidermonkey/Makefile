MODNAME=mod_spidermonkey
WANT_CURL=yes

LOCAL_CFLAGS=
LOCAL_LDFLAGS=
LOCAL_LIBADD=

include sm.mak

depend_install: $(DESTDIR)$(PREFIX)/lib/libjs.la

$(DESTDIR)$(PREFIX)/lib/libjs.la: $(JS_DIR)/nsprpub/pr/src/libnspr4.so $(JS_DIR)/nsprpub/lib/ds/libplds4.so $(JS_DIR)/nsprpub/lib/libc/src/libplc4.so $(JS_DIR)/libjs.la
	cd $(JS_DIR)/nsprpub/pr/src && $(INSTALL) libnspr4.so $(DESTDIR)$(PREFIX)/lib
	cd $(JS_DIR)/nsprpub/lib/ds && $(INSTALL) libplds4.so $(DESTDIR)$(PREFIX)/lib
	cd $(JS_DIR)/nsprpub/lib/libc/src && $(INSTALL) libplc4.so $(DESTDIR)$(PREFIX)/lib 
	cd $(JS_DIR) && $(LTINSTALL) libjs.la $(DESTDIR)$(PREFIX)/lib


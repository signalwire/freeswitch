WANT_CURL=yes

LOCAL_CFLAGS=
LOCAL_LDFLAGS=
LOCAL_LIBADD=

include sm.mak

depend_install: $(DESTDIR)$(PREFIX)/lib/libjs.la

$(DESTDIR)$(PREFIX)/lib/libjs.la: $(JSLA)
	cd $(JS_DIR)/nsprpub/ && $(MAKE) install
	cd $(JS_DIR) && $(MAKE) install


WANT_CURL=yes

LOCAL_CFLAGS=
LOCAL_LDFLAGS=
LOCAL_LIBADD=

include sm.mak

depend_install: $(DESTDIR)$(libdir)/libjs.la

$(DESTDIR)$(libdir)/libjs.la: $(JSLA)
	cd $(JS_DIR)/nsprpub/ && $(MAKE) -j1 install && cd $(JS_DIR) && $(MAKE) -j1 install


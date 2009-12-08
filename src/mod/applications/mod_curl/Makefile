json-c=json-c-0.8
BASE=../../../..
WANT_CURL=yes

JSON_DIR=$(switch_srcdir)/libs/$(json-c)
JSON_BUILDDIR=$(switch_builddir)/libs/$(json-c)

JSONLA=$(JSON_BUILDDIR)/libjson.la

LOCAL_CFLAGS=-I$(JSON_DIR)
LOCAL_LIBADD=$(JSONLA)

include $(BASE)/build/modmake.rules

$(JSON_DIR):
	$(GETLIB) $(json-c).tar.gz

$(JSON_DIR)/Makefile: $(JSON_DIR)
	cd $(JSON_BUILDDIR) && $(DEFAULT_VARS) ./configure $(DEFAULT_ARGS) CPPFLAGS= LDFLAGS=
	$(TOUCH_TARGET)

$(JSONLA): $(JSON_DIR)/Makefile
	cd $(JSON_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

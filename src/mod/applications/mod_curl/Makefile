json-c=json-c-0.9
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

$(JSON_BUILDDIR)/Makefile: $(JSON_DIR)
	mkdir -p $(JSON_BUILDDIR)
	cd $(JSON_BUILDDIR) && $(DEFAULT_VARS) $(JSON_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(JSON_DIR) CPPFLAGS= LDFLAGS=
	$(TOUCH_TARGET)

$(JSONLA): $(JSON_BUILDDIR)/Makefile
	cd $(JSON_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

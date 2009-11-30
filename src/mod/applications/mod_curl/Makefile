json-c=json-c-0.8
BASE=../../../..
WANT_CURL=yes

JSON_DIR=$(BASE)/libs/$(json-c)

JSONLA=$(JSON_DIR)/libjson.la

LOCAL_CFLAGS=-I$(JSON_DIR)
LOCAL_LIBADD=$(JSONLA)

include $(BASE)/build/modmake.rules

$(LOCAL_OBJS): $(LOCAL_SOURCES)

$(JSON_DIR):
	$(GETLIB) $(json-c).tar.gz

$(JSON_DIR)/Makefile: $(JSON_DIR)
	cd $(JSON_DIR) && $(DEFAULT_VARS) ./configure $(DEFAULT_ARGS) CPPFLAGS= LDFLAGS=
	$(TOUCH_TARGET)

$(JSONLA): $(JSON_DIR)/Makefile
	cd $(JSON_DIR) && $(MAKE)
	$(TOUCH_TARGET)

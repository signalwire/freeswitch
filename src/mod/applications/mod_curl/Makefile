json-c=json-c-0.8
BASE=../../../..
WANT_CURL=yes

JSON_DIR=$(BASE)/libs/$(json-c)

JSONLA=$(JSON_DIR)/libjson.la

LOCAL_CFLAGS=-I$(JSON_DIR)
LOCAL_LIBADD=$(JSONLA)

include $(BASE)/build/modmake.rules

DEFAULT_ARGS=--prefix=$(PREFIX) --disable-shared --with-pic

$(LOCAL_OBJS): $(LOCAL_SOURCES)

$(JSON_DIR):
	$(GETLIB) $(json-c).tar.gz

$(JSON_DIR)/Makefile: $(JSON_DIR)
	cd $(JSON_DIR) && CFLAGS=$(AM_CFLAGS) CC=$(CC) CXX=$(CXX) ./configure --disable-shared --with-pic CPPFLAGS= LDFLAGS=
	$(TOUCH_TARGET)

$(JSONLA): $(JSON_DIR)/Makefile
	cd $(JSON_DIR) && $(MAKE)
	$(TOUCH_TARGET)

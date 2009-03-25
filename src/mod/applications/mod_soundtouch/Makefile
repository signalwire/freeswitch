
VERSION=soundtouch-1.3.1
DIRECTORY=$(switch_srcdir)/libs/$(VERSION)
LA=$(DIRECTORY)/source/SoundTouch/.libs/libSoundTouch.a

BASE=../../../..
LOCAL_CFLAGS += -I$(DIRECTORY)/include -DINTEGER_SAMPLES
LOCAL_LIBADD=$(LA)
DEFAULT_ARGS=--prefix=$(PREFIX) --disable-shared --with-pic

include $(BASE)/build/modmake.rules

$(DIRECTORY):
	$(GETLIB) $(VERSION).tar.gz
	cd $(DIRECTORY) && ./configure $(DEFAULT_ARGS) --enable-integer-samples

$(LA): $(DIRECTORY)
	cd $(DIRECTORY) && $(MAKE)
	$(TOUCH_TARGET)

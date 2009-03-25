VERSION=soundtouch-1.3.1
DIRECTORY=$(switch_srcdir)/libs/$(VERSION)
LA=$(DIRECTORY)/source/SoundTouch/.libs/libSoundTouch.a

BASE=../../../..
LOCAL_CFLAGS += -I$(DIRECTORY)/include -DINTEGER_SAMPLES
LOCAL_LIBADD=$(LA)
DEFAULT_ARGS=--prefix=$(PREFIX) --disable-shared --with-pic

include $(BASE)/build/modmake.rules

$(DIRECTORY)/Makefile $(DIRECTORY)/config.status:
	$(GETLIB) $(VERSION).tar.gz
	cd $(DIRECTORY) && ./configure $(DEFAULT_ARGS) --enable-integer-samples
	$(TOUCH_TARGET)

$(LA): $(DIRECTORY)/Makefile $(DIRECTORY)/config.status
	cd $(DIRECTORY) && $(MAKE)
	$(TOUCH_TARGET)


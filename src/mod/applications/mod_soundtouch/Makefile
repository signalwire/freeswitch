VERSION=soundtouch-1.3.1
DIRECTORY=$(switch_srcdir)/libs/$(VERSION)
LA=$(DIRECTORY)/source/SoundTouch/.libs/libSoundTouch.a

BASE=../../../..
LOCAL_CFLAGS += -I$(DIRECTORY)/include -DINTEGER_SAMPLES
LOCAL_LIBADD=$(LA)

include $(BASE)/build/modmake.rules

$(DIRECTORY)/Makefile $(DIRECTORY)/config.status:
	$(GETLIB) $(VERSION).tar.gz
	cd $(DIRECTORY) && $(DEFAULT_VARS) ./configure $(DEFAULT_ARGS) --enable-integer-samples
	$(TOUCH_TARGET)

$(LA): $(DIRECTORY)/Makefile $(DIRECTORY)/config.status
	cd $(DIRECTORY) && $(MAKE)
	$(TOUCH_TARGET)


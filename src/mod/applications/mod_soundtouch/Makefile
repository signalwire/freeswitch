VERSION=soundtouch-1.3.1
DIRECTORY=$(switch_srcdir)/libs/$(VERSION)
BUILD_DIRECTORY=$(switch_builddir)/libs/$(VERSION)
LA=$(BUILD_DIRECTORY)/source/SoundTouch/.libs/libSoundTouch.a

BASE=../../../..
LOCAL_CFLAGS += -I$(DIRECTORY)/include -DINTEGER_SAMPLES -I$(BUILD_DIRECTORY)/include
LOCAL_LIBADD=$(LA)

include $(BASE)/build/modmake.rules

$(BUILD_DIRECTORY)/Makefile $(BUILD_DIRECTORY)/config.status:
	$(GETLIB) $(VERSION).tar.gz
	mkdir -p $(BUILD_DIRECTORY)
	cd $(BUILD_DIRECTORY) && $(DEFAULT_VARS) $(DIRECTORY)/configure $(DEFAULT_ARGS) --enable-integer-samples --srcdir=$(DIRECTORY)
	$(TOUCH_TARGET)

$(LA): $(BUILD_DIRECTORY)/Makefile $(BUILD_DIRECTORY)/config.status
	cd $(BUILD_DIRECTORY) && $(MAKE)
	$(TOUCH_TARGET)


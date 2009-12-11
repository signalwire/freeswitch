BASE=../../../..
G722_1_DIR=$(switch_srcdir)/libs/libg722_1
G722_1_BUILDDIR=$(switch_builddir)/libs/libg722_1
LOCAL_CFLAGS=-I$(G722_1_DIR)/src  -I$(G722_1_BUILDDIR)/src
G722_1_LA=$(G722_1_BUILDDIR)/src/.libs/libg722_1.la
LOCAL_LIBADD=$(G722_1_LA)
include $(BASE)/build/modmake.rules

$(G722_1_LA): $(G722_1_DIR)/.update	
	cd $(G722_1_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

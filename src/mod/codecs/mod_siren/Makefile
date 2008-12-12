BASE=../../../..
G722_1_DIR=$(BASE)/libs/libg722_1
LOCAL_CFLAGS=-I$(G722_1_DIR)/src 
G722_1_LA=$(G722_1_DIR)/src/.libs/libg722_1.la
LOCAL_LIBADD=$(G722_1_LA)
include $(BASE)/build/modmake.rules

$(G722_1_LA): $(G722_1_DIR)/.update	
	cd $(G722_1_DIR) && ./configure  --disable-shared --with-pic  && $(MAKE)
	$(TOUCH_TARGET)

# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build

BASE=../../../..

IKSEMEL_DIR=$(BASE)/libs/iksemel
IKSEMELLA=$(IKSEMEL_DIR)/src/libiksemel.la

LOCAL_CFLAGS=-I$(IKSEMEL_DIR)/include
LOCAL_LIBADD=$(IKSEMELA)
LOCAL_OBJS=
include $(BASE)/build/modmake.rules

$(IKSEMELLA): $(IKSEMEL_DIR) $(IKSEMEL_DIR)/.update
	cd $(IKSEMEL_DIR) && $(MAKE)
	$(TOUCH_TARGET)

BASE=../../..
OZ_DIR=..
VERBOSE=1
OZA=$(OZ_DIR)/libopenzap.a
LOCAL_CFLAGS=-I$(OZ_DIR)/src/include -I$(OZ_DIR)/src/isdn/include
LOCAL_LIBADD=$(OZA)
include $(BASE)/build/modmake.rules

local_depend: $(OZA)

$(OZA): $(OZ_DIR)/.update
	cd $(OZ_DIR) && $(MAKE) ZAP_MODS="wanpipe zt" ZAP_CFLAGS="$(ALL_CXXFLAGS)"



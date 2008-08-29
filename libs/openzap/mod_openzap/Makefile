BASE=../../..
OZ_DIR=..
VERBOSE=1
OZSO=$(OZ_DIR)/libopenzap.so
LOCAL_CFLAGS=-I$(OZ_DIR)/src/include -I$(OZ_DIR)/src/isdn/include
LOCAL_LDFLAGS=-L$(OZ_DIR) -lopenzap 
include $(BASE)/build/modmake.rules

local_depend: $(OZSO)

$(OZSO): $(OZ_DIR)/.update
	cd $(OZ_DIR) && $(MAKE) ZAP_CFLAGS="$(ALL_CXXFLAGS)"

local_install:
	cd $(OZ_DIR) && $(MAKE) install ZAP_CFLAGS="$(ALL_CXXFLAGS)"
	[ ! -f $(DESTDIR)$(PREFIX)/conf/autoload_configs/openzap.conf.xml ] || cp -f $(OZ_DIR)/conf/openzap.conf.xml  $(DESTDIR)$(PREFIX)/conf/autoload_configs

local_clean:
	cd $(OZ_DIR) && $(MAKE) clean



MODNAME=mod_spidermonkey
include sm.mak
LOCAL_CFLAGS  += -I$(BASE)/libs/curl/include
LOCAL_LDFLAGS += $(BASE)/libs/curl/lib/libcurl.la

include ../../../../build/modmake.rules

local_depend:
	cd $(BASE)/libs/curl && $(MAKE)
	cd $(BASE)/libs/js/nsprpub && $(MAKE)
	cd $(BASE)/libs/js && $(MAKE)

depend_install:
	cd $(BASE)/libs/js/nsprpub && $(MAKE) install
	cd $(BASE)/libs/js && $(MAKE) install
	$(LINK) $(SOLINK) $(MODNAME).o -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(LIBS) $(LOCAL_LDFLAGS) $(LOCAL_OBJS)


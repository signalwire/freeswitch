# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# TODO
# - needs to compile/link against the _freeswitch_ sofia libs (see FIXME)

OPENMRCP_DIR=../../../../libs/openmrcp

OPENMRCP_INCLUDE=-I$(OPENMRCP_DIR)/mrcpcore/client/include/ -I$(OPENMRCP_DIR)/platform/openmrcpclient/include/ -I$(OPENMRCP_DIR)/mediaframe/include/ -I$(OPENMRCP_DIR)/mrcpcore/engine/include/ -I$(OPENMRCP_DIR)/mrcpcore/include/ -I$(OPENMRCP_DIR)/mrcpcore/parser/include/ -I$(OPENMRCP_DIR)/mrcpcore/server/include/ -I$(OPENMRCP_DIR)/mrcpcore/media/include/ -I$(OPENMRCP_DIR)/mrcpcore/util/include -I$(OPENMRCP_DIR)/mrcpcore/resource/include/

OPENMRCP_LIBS=$(OPENMRCP_DIR)/platform/openmrcpclient/.libs/libopenmrcpclient.a $(OPENMRCP_DIR)/platform/openmrcpserver/.libs/libopenmrcpserver.a $(OPENMRCP_DIR)/mrcpcore/client/.libs/libmrcpclient.a $(OPENMRCP_DIR)/mediaframe/.libs/libmediaframe.a $(OPENMRCP_DIR)/mrcpcore/engine/.libs/libmrcpengine.a $(OPENMRCP_DIR)/mrcpcore/parser/.libs/libmrcpparser.a $(OPENMRCP_DIR)/mrcpcore/server/.libs/libmrcpserver.a $(OPENMRCP_DIR)/mrcpcore/media/.libs/libmrcpmedia.a $(OPENMRCP_DIR)/mrcpcore/util/.libs/libmrcputil.a $(OPENMRCP_DIR)/mrcpcore/resource/.libs/libmrcpresource.a $(OPENMRCP_DIR)/module/plugin/demo/.libs/libdemoplugin.a $(OPENMRCP_DIR)/module/mrcpv2agent/.libs/libmrcpv2agent.a $(OPENMRCP_DIR)/module/mrcpmediaframe/.libs/libmrcpmediaframe.a $(OPENMRCP_DIR)/module/mrcpsofiasip/.libs/libmrcpsofiasip.a $(OPENMRCP_DIR)/mrcpcore/.libs/libmrcpcore.a

MRCP_APR_INCLUDES =  -I../../../../libs/apr/include
MRCP_APU_INCLUDES =  -I../../../../libs/apr-util/include

#do not specify gcc specific compiler flags
#LOCAL_CFLAGS= -Wno-unused -Wno-comment -Wno-sign-compare
LOCAL_CFLAGS= $(OPENMRCP_INCLUDE) $(MRCP_APR_INCLUDES) $(MRCP_APU_INCLUDES) 

LOCAL_LIBADD=$(OPENMRCP_LIBS)

LOCAL_INSERT_LDFLAGS=if test $$osarch = "Darwin" ; then echo "-framework CoreFoundation -framework SystemConfiguration -lssl -lcrypto" ; fi ;

SOFIA_DIR=../../../../libs/sofia-sip
SOFIAUA_DIR=$(SOFIA_DIR)/libsofia-sip-ua
SOFIALA=$(SOFIAUA_DIR)/libsofia-sip-ua.la

include ../../../../build/modmake.rules

$(OPENMRCP_DIR)/.built: $(OPENMRCP_DIR)/.update
	cd $(OPENMRCP_DIR) && ./bootstrap && ./configure --with-apr=../apr --with-apr-util=../apr-util --with-sofia-sip=../sofia-sip
	cd $(OPENMRCP_DIR) && $(MAKE) && touch .built

$(OPENMRCP_LIBS): $(SOFIALA) $(OPENMRCP_DIR)/.built

$(SOFIALA): $(SOFIA_DIR) $(SOFIA_DIR)/.update
	cd $(SOFIA_DIR) && $(MAKE)
	$(TOUCH_TARGET)

../../../../libs/sofia-sip/libsofia-sip-ua/nua/sofia-sip/nua_tag.h: $(SOFIALA)



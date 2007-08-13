# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build

# TO GET THIS TO COMPILE
# hack ../../../../build/modmake.rules and remove the
# -Werror from the ALL_CFLAGS variable.  

# TODO
# - re-enable the -Werror flag after openmrcp compilation warnings are fixed
# - needs to compile/link against the _freeswitch_ sofia libs (see FIXME)
# - Should this download and build openmrcp?  What's the plan? (see FIXME)
# - somehow consolidate libs and includes into less files

# FIXME!!!  
OPENMRCP_DIR=/usr/src/openmrcp_trunk

OPENMRCP_INCLUDE=-I$(OPENMRCP_DIR)/mrcpcore/client/include/ -I$(OPENMRCP_DIR)/platform/openmrcpclient/include/ -I$(OPENMRCP_DIR)/mediaframe/include/ -I$(OPENMRCP_DIR)/mrcpcore/engine/include/ -I$(OPENMRCP_DIR)/mrcpcore/include/ -I$(OPENMRCP_DIR)/mrcpcore/parser/include/ -I$(OPENMRCP_DIR)/mrcpcore/server/include/ -I$(OPENMRCP_DIR)/mrcpcore/media/include/ -I$(OPENMRCP_DIR)/mrcpcore/util/include -I$(OPENMRCP_DIR)/mrcpcore/resource/include/

OPENMRCP_LIBS=$(OPENMRCP_DIR)/platform/openmrcpclient/.libs/libopenmrcpclient.a $(OPENMRCP_DIR)/platform/openmrcpserver/.libs/libopenmrcpserver.a $(OPENMRCP_DIR)/mrcpcore/client/.libs/libmrcpclient.a $(OPENMRCP_DIR)/mediaframe/.libs/libmediaframe.a $(OPENMRCP_DIR)/mrcpcore/engine/.libs/libmrcpengine.a $(OPENMRCP_DIR)/mrcpcore/parser/.libs/libmrcpparser.a $(OPENMRCP_DIR)/mrcpcore/server/.libs/libmrcpserver.a $(OPENMRCP_DIR)/mrcpcore/media/.libs/libmrcpmedia.a $(OPENMRCP_DIR)/mrcpcore/util/.libs/libmrcputil.a $(OPENMRCP_DIR)/mrcpcore/util/.libs/libmrcputil.a $(OPENMRCP_DIR)/mrcpcore/resource/.libs/libmrcpresource.a $(OPENMRCP_DIR)/module/plugin/demo/.libs/libdemoplugin.a $(OPENMRCP_DIR)/module/mrcpv2agent/.libs/libmrcpv2agent.a $(OPENMRCP_DIR)/module/mrcpmediaframe/.libs/libmrcpmediaframe.a $(OPENMRCP_DIR)/module/mrcpsofiasip/.libs/libmrcpsofiasip.a $(OPENMRCP_DIR)/mrcpcore/.libs/libmrcpcore.a

MRCP_APR_INCLUDES =  -I../../../../libs/apr/include
MRCP_APU_INCLUDES =  -I../../../../libs/apr-util/include
MRCP_SOFIA_INCLUDES = -I/usr/local/include/sofia-sip-1.12  # FIXME!!

LOCAL_CFLAGS= -Wno-unused
LOCAL_CFLAGS+= -Wno-comment -Wno-sign-compare $(OPENMRCP_INCLUDE) $(MRCP_APR_INCLUDES) $(MRCP_APU_INCLUDES) $(MRCP_SOFIA_INCLUDES)

LOCAL_LDFLAGS=$(OPENMRCP_LIBS)
VERBOSE=TRUE
include ../../../../build/modmake.rules


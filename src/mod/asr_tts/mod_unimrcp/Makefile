BASE=../../../../

UNIMRCP_DIR=$(switch_srcdir)/libs/unimrcp
UNIMRCPLA=$(UNIMRCP_DIR)/platforms/libunimrcp-client/libunimrcpclient.la
LOCAL_CFLAGS=-I.
LOCAL_CFLAGS+=-I$(switch_srcdir)/libs/apr/include
LOCAL_CFLAGS+=-I$(switch_srcdir)/libs/apr-util/include
LOCAL_CFLAGS+=-I$(UNIMRCP_DIR)/platforms/libunimrcp-client/include
LOCAL_CFLAGS+=-I$(UNIMRCP_DIR)/modules/mrcp-sofiasip/include
LOCAL_CFLAGS+=-I$(UNIMRCP_DIR)/modules/mrcp-unirtsp/include
LOCAL_CFLAGS+=-I$(UNIMRCP_DIR)/libs/mrcp-client/include
LOCAL_CFLAGS+=-I$(UNIMRCP_DIR)/libs/mrcp-signaling/include
LOCAL_CFLAGS+=-I$(UNIMRCP_DIR)/libs/mrcpv2-transport/include
LOCAL_CFLAGS+=-I$(UNIMRCP_DIR)/libs/mrcp/include
LOCAL_CFLAGS+=-I$(UNIMRCP_DIR)/libs/mrcp/message/include
LOCAL_CFLAGS+=-I$(UNIMRCP_DIR)/libs/mrcp/control/include
LOCAL_CFLAGS+=-I$(UNIMRCP_DIR)/libs/mrcp/resources/include
LOCAL_CFLAGS+=-I$(UNIMRCP_DIR)/libs/mpf/include
LOCAL_CFLAGS+=-I$(UNIMRCP_DIR)/libs/apr-toolkit/include
LOCAL_LIBADD=$(UNIMRCPLA)

include $(BASE)/build/modmake.rules

$(UNIMRCPLA): $(UNIMRCP_DIR) $(UNIMRCP_DIR)/.update
	cd $(UNIMRCP_DIR) && $(MAKE)
	$(TOUCH_TARGET)


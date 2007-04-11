BASE=../../../..
UDNS_DIR=$(BASE)/libs/udns
LOCAL_CFLAGS=-I$(UDNS_DIR) -DHAVE_POLL
LOCAL_OBJS=$(UDNS_DIR)/udns_dn.o $(UDNS_DIR)/udns_dntosp.o $(UDNS_DIR)/udns_parse.o $(UDNS_DIR)/udns_misc.o \
$(UDNS_DIR)/udns_rr_a.o $(UDNS_DIR)/udns_rr_ptr.o $(UDNS_DIR)/udns_rr_mx.o $(UDNS_DIR)/udns_rr_txt.o $(UDNS_DIR)/udns_bl.o \
$(UDNS_DIR)/udns_rr_srv.o $(UDNS_DIR)/udns_rr_naptr.o $(UDNS_DIR)/udns_codes.o $(UDNS_DIR)/udns_resolver.o


include $(BASE)/build/modmake.rules


BASE=../../../..

AMR_DIR=$(BASE)/libs/amrwb
AMR_A=$(AMR_DIR)/libamrwb.a

LOCAL_INSERT_CFLAGS=if test -d $(AMR_DIR); then echo "-I$(AMR_DIR)" ; else echo "-DAMRWB_PASSTHROUGH" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -d $(AMR_DIR) || echo "$(AMR_A)";


include $(BASE)/build/modmake.rules

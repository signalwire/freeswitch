BASE=../../../..

AMR_DIR=$(BASE)/libs/amr
AMR_A=$(AMR_DIR)/libamr.a

LOCAL_INSERT_CFLAGS=if test -d $(AMR_DIR); then echo "-I$(AMR_DIR)" ; else echo "-DAMR_PASSTHROUGH" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -d $(AMR_DIR) || echo "$(AMR_A)";


include $(BASE)/build/modmake.rules

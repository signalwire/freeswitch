BASE=../../../..

AMR_DIR=$(switch_srcdir)/libs/amrwb
AMR_BUILDDIR=$(switch_builddir)/libs/amrwb
AMR_A=$(AMR_BUILDDIR)/libamrwb.a

LOCAL_INSERT_CFLAGS=if test -d $(AMR_DIR); then echo "-I$(AMR_DIR)" ; else echo "-DAMRWB_PASSTHROUGH" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -d $(AMR_DIR) || echo "$(AMR_A)";


include $(BASE)/build/modmake.rules

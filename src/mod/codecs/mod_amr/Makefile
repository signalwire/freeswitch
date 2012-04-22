BASE=../../../..

AMR_DIR=$(switch_srcdir)/libs/amr
AMR_BUILDDIR=$(switch_builddir)/libs/amr
AMR_A=$(AMR_BUILDDIR)/libamr.a

LOCAL_INSERT_CFLAGS=if test -d $(AMR_DIR); then echo "-I$(AMR_DIR)" ; else echo "-DAMR_PASSTHROUGH" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -d $(AMR_DIR) || echo "$(AMR_A)";


include $(BASE)/build/modmake.rules

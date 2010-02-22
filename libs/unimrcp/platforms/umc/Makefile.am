MAINTAINERCLEANFILES   = Makefile.in

INCLUDES               = -Iinclude \
                         -I$(top_srcdir)/platforms/libunimrcp-client/include \
                         -I$(top_srcdir)/libs/mrcp-client/include \
                         -I$(top_srcdir)/libs/mrcp-signaling/include \
                         -I$(top_srcdir)/libs/mrcpv2-transport/include \
                         -I$(top_srcdir)/libs/mrcp/include \
                         -I$(top_srcdir)/libs/mrcp/message/include \
                         -I$(top_srcdir)/libs/mrcp/control/include \
                         -I$(top_srcdir)/libs/mrcp/resources/include \
                         -I$(top_srcdir)/libs/mpf/include \
                         -I$(top_srcdir)/libs/apr-toolkit/include \
                         $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

bin_PROGRAMS           = umc
umc_SOURCES            = src/main.cpp \
                         src/umcconsole.cpp \
                         src/umcframework.cpp \
                         src/umcscenario.cpp \
                         src/umcsession.cpp \
                         src/synthscenario.cpp \
                         src/synthsession.cpp \
                         src/recogscenario.cpp \
                         src/recogsession.cpp \
                         src/recorderscenario.cpp \
                         src/recordersession.cpp \
                         src/dtmfscenario.cpp \
                         src/dtmfsession.cpp \
                         src/setparamscenario.cpp \
                         src/setparamsession.cpp
umc_LDADD              = $(top_builddir)/platforms/libunimrcp-client/libunimrcpclient.la

if ISMAC
umc_LDFLAGS            = -framework CoreFoundation -framework SystemConfiguration
endif

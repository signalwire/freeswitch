AM_CPPFLAGS            = -I$(top_srcdir)/platforms/umc/include \
                         $(UNIMRCP_CLIENTAPP_INCLUDES)

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
                         src/setparamsession.cpp \
                         src/verifierscenario.cpp \
                         src/verifiersession.cpp
umc_LDADD              = $(UNIMRCP_CLIENTAPP_LIBS)
umc_LDFLAGS            = $(UNIMRCP_CLIENTAPP_OPTS)

include $(top_srcdir)/build/rules/uniclientapp.am

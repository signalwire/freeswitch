BASE=../../../..

A=../../../../libs/silk/libSKP_SILK_SDK.a

LOCAL_INSERT_CFLAGS=if test -f $(A); then echo "-I../../../../libs/silk/interface" ; else echo "-DNOTFOUND" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -f $(A) || echo $(A)

include $(BASE)/build/modmake.rules

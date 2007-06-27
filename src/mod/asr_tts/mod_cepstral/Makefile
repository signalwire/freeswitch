LOCAL_INSERT_LDFLAGS=if test $$osarch = "Darwin" ; then echo "-framework swift" ; else echo "-L$(SWIFT_HOME)/lib -lswift -lceplex_us -lceplang_en -lm" ; fi ;

LOCAL_INSERT_CFLAGS=test $$osarch = "Darwin" || echo "-I$(SWIFT_HOME)/include";
LOCAL_CFLAGS=-w
include ../../../../build/modmake.rules


LOCAL_INSERT_LDFLAGS=if test $$osarch = "Darwin" ; then echo "-framework swift" ; else echo "-L$(SWIFT_HOME)/lib -L/opt/swift/lib -lswift -lm" ; fi ;

LOCAL_INSERT_CFLAGS=test $$osarch = "Darwin" || echo "-I$(SWIFT_HOME)/include -I/opt/swift/include";
LOCAL_CFLAGS=-w
include ../../../../build/modmake.rules


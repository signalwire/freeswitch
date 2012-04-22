LOCAL_INSERT_LDFLAGS=if test $$osarch = "Darwin" ; then echo "-framework swift" ; else echo "-L$(SWIFT_HOME)/lib -L/opt/swift/lib -lswift -lm" ; fi ;

LOCAL_INSERT_CFLAGS=if test $$osarch = "Darwin" ; then echo "-I/Library/Frameworks/swift.framework/Versions/5/Headers/" ; else echo "-I$(SWIFT_HOME)/include -I/opt/swift/include"; fi ;
LOCAL_CFLAGS=-w
include ../../../../build/modmake.rules


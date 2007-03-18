LOCAL_INSERT_LDFLAGS=if test $$osarch = "Darwin" ; then echo "-framework swift" ; else echo "-L/opt/swift/lib -lswift -lceplex_us -lceplang_en -lm" ; fi ;

LOCAL_INSERT_CFLAGS=test $$osarch = "Darwin" || echo "-I/opt/swift/include";

include ../../../../build/modmake.rules


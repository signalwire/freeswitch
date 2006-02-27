cd nsprpub && ./configure && make
cd ../js/src && JS_THREADSAFE=1 OTHER_LIBS="-L../../../mozilla/nsprpub/dist/lib" INCLUDES="-I../../../mozilla/nsprpub/dist/include/nspr"  make -f Makefile.ref $(OS_CONFIG)_DBG.OBJ/libjs.a


arch=`uname -m`

opts=""
if [ $arch = "x86_64" ] ; then
opts="--enable-64bit"
fi

cd nsprpub && ./configure $opts && $MAKE

cd ../js/src && JS_THREADSAFE=1 JS_HAS_FILE_OBJECT=1 OTHER_LIBS="-L../../../mozilla/nsprpub/dist/lib" INCLUDES="-I../../../mozilla/nsprpub/dist/include/nspr"  $MAKE -f Makefile.ref `find . -name libjs.a`


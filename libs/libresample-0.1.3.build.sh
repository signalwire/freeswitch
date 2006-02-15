./configure $@
make
cp *.a $PREFIX/lib
cp include/* $PREFIX/include
ranlib $PREFIX/lib/libresample.a

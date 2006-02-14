./configure $@
make
*.a $PREFIX/lib
include/* $PREFIX/include
ranlib $PREFIX/lib/libresample.a

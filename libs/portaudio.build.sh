arch=`uname -m`

opts=""
if [ $arch = "x86_64" ] ; then
opts="-fPIC"
fi

CFLAGS=$opts ./configure $@
make
make install





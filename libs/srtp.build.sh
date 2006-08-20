arch=`uname -m`

opts=""
if [ $arch = "x86_64" ] ; then
opts="--enable-pic"
fi

./configure $@ $opts

$MAKE clean uninstall all
$MAKE install







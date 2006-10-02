arch=`uname -m`

opts=""
if [ $arch = "x86_64" ] ; then
opts="--enable-pic"
fi

./configure $@ $opts

$MAKE clean
$MAKE uninstall
$MAKE 
$MAKE install







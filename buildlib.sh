#!/bin/bash
base=http://www.sofaswitch.com/mikej
tar=$1
uncompressed=`echo $tar | sed "s/\.tar\.gz//g"`
if [ -d libs/$uncompressed ] ; then
    echo $uncompressed already installed
    exit
fi
shift
cd libs
rm -f $tar
wget $base/$tar
tar -zxvf $tar
echo "lame $uncompressed"
cd $uncompressed
./configure $@
make
make install



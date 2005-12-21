#!/bin/bash
install=
base=http://www.sofaswitch.com/mikej
tar=$1

shift

if [ ! -z $1 ] && [ $1 = install ] ; then
    install=1
    shift
fi

cd libs

if [ -d $tar ] ; then
    uncompressed=$tar
    tar=
else
    uncompressed=`echo $tar | sed "s/\.tar\.gz//g"`
    if [ ! -f $tar ] ; then
	rm -fr $uncompressed
	wget $base/$tar
	if [ ! -f $tar ] ; then
	    echo cannot find $tar
	    exit
	fi
	tar -zxvf $tar
    fi
fi

if [ -f $uncompressed/.complete ] && [ -z $install ] ; then
    echo $uncompressed already installed
    exit
fi

cd $uncompressed

./configure $@

if [ $? == 0 ] ; then
    make
else 
    echo ERROR
    exit 1
fi

if [ ! -z $install ] ; then
    make install
fi

if [ $? == 0 ] ; then
    touch .complete
else 
    echo ERROR
    exit 1
fi

exit 0

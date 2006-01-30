#!/bin/sh

root=$1
shift

if [ -f $root/.nodepends ] ; then
    echo "***depends disabled*** use $MAKE yesdepends to re-enable"
    exit 0
fi

if [ -z $MAKE ] ; then
    make=`which dmake 2>/dev/null`
    if [ -z $MAKE ] ; then
	make=make
    fi
fi

install=
base=http://www.freeswitch.org/downloads/libs

if [ ! -z $1 ] && [ $1 = install ] ; then
    install=1
    shift
fi

tar=$1
shift

cd $root/libs/.
CFLAGS=
LDFLAGS=
MAKEFLAGS=

if [ -d $tar ] ; then
    uncompressed=$tar
    tar=
else
    uncompressed=`echo $tar | sed "s/\.tar\.gz//g"`
    uncompressed=`echo $uncompressed | sed "s/\.tgz//g"`

    if [ ! -f $tar ] ; then
	rm -fr $uncompressed
	wget $base/$tar
	if [ ! -f $tar ] ; then
	    echo cannot find $tar
	    exit
	fi
    fi
    if [ ! -d $uncompressed ] ; then
	tar -zxvf $tar
    fi
fi

if [ -f $uncompressed/.complete ] ; then
    echo $uncompressed already installed
    exit 0
fi

cd $uncompressed
$MAKE clean 2>&1
sh ./configure $@

if [ $? = 0 ] ; then
    $MAKE
else 
    echo ERROR
    exit 1
fi

if [ ! -z $install ] ; then
    $MAKE install
fi

if [ $? = 0 ] ; then
    touch .complete
else 
    echo ERROR
    exit 1
fi

exit 0

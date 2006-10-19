#!/bin/sh

root=$1
shift


if [ -f $root/.nodepends ] ; then
    echo "***depends disabled*** use $MAKE yesdepends to re-enable"
    exit 0
fi

if [ -f $root/build/freeswitch.env ] ; then
    . $root/build/freeswitch.env
fi

if [ -z "$MAKE" ] ; then
    make=`which gmake 2>/dev/null`
    if [ -z "$MAKE" ] ; then
	make=make
    fi
fi

install=
base=http://svn.freeswitch.org/downloads/libs

if [ ! -z "$1" ] && [ "$1" = install ] ; then
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
	wget $base/$tar || ftp $base/$tar
	if [ ! -f $tar ] ; then
	    echo cannot find $tar
	    exit
	fi
    fi
    if [ ! -d $uncompressed ] ; then
	tar -zxvf $tar
    fi
fi

if [ ! -f $root/.nothanks ] && [ $uncompressed/.complete -ot $uncompressed ] ; then 
    echo remove stale .complete
    rm $uncompressed/.complete
    sh -c "cd $uncompressed && $MAKE clean distclean"
fi

if [ -f $uncompressed/.complete ] ; then
    echo $uncompressed already installed
    exit 0
fi

cd $uncompressed

if [ -f ../$uncompressed.build.sh ] ; then
    MAKE=$MAKE ../$uncompressed.build.sh $@
else
    $MAKE clean 2>&1
    CFLAGS="$MOD_CFLAGS" ; export CFLAGS; sh ./configure $@

    if [ $? = 0 ] ; then
	$MAKE
    else 
	echo ERROR
	exit 1
    fi

    if [ ! -z $install ] ; then
	$MAKE install
    fi
fi

if [ $? = 0 ] ; then
    touch .complete
else 
    echo ERROR
    exit 1
fi

exit 0

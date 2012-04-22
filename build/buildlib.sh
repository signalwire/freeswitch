#!/bin/sh

root=$1
shift


if [ -f $root/.nodepends ] ; then
    echo "***depends disabled*** use $MAKE yesdepends to re-enable"
    exit 0
fi

if [ -z "$MAKE" ] ; then
    make=`which gmake 2>/dev/null`
    if [ -z "$MAKE" ] ; then
	make=make
    fi
fi

GZCAT=`which gzcat 2>/dev/null`
if [ -z "$GZCAT" ] ; then
    GZCAT=zcat
fi

install=
base=http://files.freeswitch.org/downloads/libs

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
	$GZCAT $tar | tar xf -
    fi
fi
if [ -f $uncompressed/.complete ] ; then 
if [ $uncompressed/.complete -ot $uncompressed ]; then
if [ ! -f $root/.nothanks ] ; then 
    echo remove stale .complete
    rm $uncompressed/.complete
    sh -c "cd $uncompressed && $MAKE clean distclean"
fi
fi
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
    CFLAGS="$MOD_CFLAGS" sh ./configure $@

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
    sleep 1
    touch .complete
else 
    echo ERROR
    exit 1
fi

exit 0


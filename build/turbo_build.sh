arg=$1 ; shift
switch_srcdir=`pwd`
if [ -z $MAKE ] ; then
    MAKE=make
fi

if [ -z $arg ] ; then
    $MAKE clean
    $MAKE -j freeswitch
    MODS=`cat modules.conf | grep -v \#`
    for i in $MODS ; do 
	MOD_NAME=`echo $i | sed -e 's|^.*/||'`
	MOD_DIR=`if test -d $switch_srcdir/src/mod/$i ; then echo $switch_srcdir/src/mod/$i ; else echo $i ; fi;`
	touch /tmp/$MOD_NAME.tmp
	$0 $MOD_NAME $MOD_DIR
    done
    while [ 1 = 1 ] ; do
	x=0
	for i in $MODS ; do
	    if [ -f /tmp/$MOD_NAME.tmp ] ; then
		x=$[$x+1];
	    fi
	done
	if [ $x = 0 ] ; then
	    echo Build finished. Making install
	    $MAKE install
	    exit;
	fi
	sleep 1
    done

    exit
fi

if [ ! $arg = "bg" ] ; then
    $0 bg $arg $@ &
    exit
fi


MOD_NAME=$1 ; shift
MOD_DIR=$1 ; shift

echo "Making module in $MOD_NAME"
if [ -f $MOD_DIR/Makefile ] ; then 
    cd $MOD_DIR && BASE=$switch_srcdir $MAKE -j
else
    cd $MOD_DIR && BASE=$switch_srcdir $MAKE -j -f $switch_srcdir/build/modmake.rules
fi

echo "Finished making module in $MOD_NAME"
sleep 1
rm /tmp/$MOD_NAME.tmp



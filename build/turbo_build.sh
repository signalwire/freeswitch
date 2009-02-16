arg=$1 ; shift
prefix=`cat config.log | grep ^prefix=\' | awk -F \' '{print $2}'`
if [ -z $prefix ] ; then
    prefix="/usr/local/freeswitch"
fi
unset VERBOSE
switch_srcdir=`pwd`
if [ -z $MAKE ] ; then
    MAKE=make
fi

if [ -z $arg ] ; then
    echo "Cleaning tree...."
    $MAKE clean modwipe 2>&1 > /dev/null
    echo "Building core...."
    $MAKE -j freeswitch 2>&1 > /dev/null
    MODS=`cat modules.conf | grep -v \#`
    for i in $MODS ; do 
	MOD_NAME=`echo $i | sed -e 's|^.*/||'`
	MOD_DIR=`if test -d $switch_srcdir/src/mod/$i ; then echo $switch_srcdir/src/mod/$i ; else echo $i ; fi;`
	touch /tmp/$MOD_NAME.tmp
	$0 $MOD_NAME $MOD_DIR
    done
    echo "Building mods...."
    while [ 1 = 1 ] ; do
	x=0
	for i in $MODS ; do
	    if [ -f /tmp/$MOD_NAME.tmp ] ; then
		x=$[$x+1];
	    fi
	done
	if [ $x = 0 ] ; then
	    echo Build finished. Making install
	    $MAKE -j install  2>&1 > /dev/null
	    echo done
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

#echo "Making module in $MOD_NAME..."
if [ -f $MOD_DIR/Makefile ] ; then 
    cd $MOD_DIR && BASE=$switch_srcdir $MAKE -j 2>&1 > /dev/null
else
    cd $MOD_DIR && BASE=$switch_srcdir $MAKE -j -f $switch_srcdir/build/modmake.rules  2>&1 > /dev/null
fi

#echo "Finished making module in $MOD_NAME"
sleep 1
rm /tmp/$MOD_NAME.tmp



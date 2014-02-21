#!/bin/sh

PATH=$PATH:/bin:/usr/bin
mods=$1

if [ -z $mods ] ; then
    mods="/usr/local/freeswitch/mod"
fi


echo "Checking module integrity in target [$mods]"
echo


here=`pwd`

cd $mods
files=`ls *.dylib *.so 2>/dev/null`
cd $here

alert() {

    /bin/echo -ne "\e[00;35m"
    echo WARNING: $1
    /bin/echo -ne '\e[00m'
}

for i in $files ; do
    mod=${i%%.*}

    infile=`grep ^.*$mod\$ ../modules.conf`
    commented=`grep ^\#.*$mod\$ ../modules.conf`

    if [ -z "$infile" ] ; then
	alert "installed module: $i was not installed by this build.  It is not present in modules.conf."
    elif [ -n "$commented" ] ; then
	alert "installed module: $i was not installed by this build.  It is commented from modules.conf. [$commented]"
    fi

done


echo

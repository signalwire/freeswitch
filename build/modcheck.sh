#!/bin/sh

PATH=$PATH:/bin:/usr/bin
mods=$1
on='[01;00;35m'
off='[0m'

if [ -z $mods ] ; then
    mods="/usr/local/freeswitch/mod"
fi

echo "Checking module integrity in target [$mods]"
echo

here=`pwd`

cd $mods
files=`ls *.so 2>/dev/null`
cd $here

for i in $files ; do
    mod=${i%%.*}

    infile=`grep ^.*$mod\$ ../modules.conf | grep -v ftmod_`
    commented=`grep ^\#.*$mod\$ ../modules.conf | grep -v ftmod_`

    if [ -z "$infile" ] ; then
	echo "${on}WARNING: installed module: $i was not installed by this build.  It is not present in modules.conf.${off}"
    elif [ -n "$commented" ] ; then
	echo "${on}WARNING: installed module: $i was not installed by this build.  It is commented from modules.conf. [$commented]${off}"
    fi

done


echo

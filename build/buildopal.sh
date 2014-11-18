#!/bin/sh

if [ -z "$1" ]; then
  INSTALLDIR=/usr/local
else
  INSTALLDIR="$1"
fi

if [ -z `which svn` ]; then
  echo "Need SVN installed!"
  exit 1
fi

uname -a | grep -qi bsd && MAKE=gmake || MAKE=make

#Locate our script, then go up one directory to be in FreeSWITCH root
cd `dirname $0`
cd ..
FS_DIR=`pwd`

export PKG_CONFIG_PATH=$INSTALLDIR/lib/pkgconfig 


# Version and patch for PTLib and OPAL. These are almost always in lock
# step so should be the same unless you really know what you are doing!
# The PATCH should be set to a  specific"snapshot release" when things
# are nice and stable. But right now comment them out for using trunk.

#VERSION=10
#PATCH=7

if [ -z "$VERSION" ]; then
  PTLIB_VERSION=trunk
  OPAL_VERSION=trunk
elif [ -z "$PATCH" ]; then
  PTLIB_VERSION=branches/v2_$VERSION
  OPAL_VERSION=branches/v3_$VERSION
else
  PTLIB_VERSION=tags/v2_${VERSION}_$PATCH
  OPAL_VERSION=tags/v3_${VERSION}_$PATCH
fi


cd $FS_DIR/libs
svn co https://svn.code.sf.net/p/opalvoip/code/ptlib/$PTLIB_VERSION ptlib
cd $FS_DIR/libs/ptlib
# LDAP disabled due to conflict wit libs in spidermonkey
./configure --disable-plugins --disable-openldap --prefix=$INSTALLDIR
${MAKE}
sudo ${MAKE} install

cd $FS_DIR/libs
svn co https://svn.code.sf.net/p/opalvoip/code/opal/$OPAL_VERSION opal 
cd $FS_DIR/libs/opal
./configure --disable-plugins --prefix=$INSTALLDIR
$MAKE
sudo $MAKE install

echo "======================================"
echo "PTLib/OPAL build and install completed"
echo "======================================"

cd $FS_DIR
$MAKE mod_opal-install

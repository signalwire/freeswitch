#!/bin/sh
uname -a | grep -qi bsd && MAKE=gmake || MAKE=make
FS_DIR=`pwd`

cd /root
svn co https://opalvoip.svn.sourceforge.net/svnroot/opalvoip/ptlib/trunk ptlib
cd ptlib
./configure --prefix=/usr
${MAKE}
${MAKE} install

cd ..
svn co https://opalvoip.svn.sourceforge.net/svnroot/opalvoip/opal/branches/v3_6 opal 
cd opal
export PKG_CONFIG_PATH=/usr/lib/pkgconfig 
./configure --prefix=/usr
${MAKE}
${MAKE} install
cd ${FS_DIR}
${MAKE} mod_opal-install
